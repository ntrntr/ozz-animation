//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "ozz/animation/offline/tools/convert2anim.h"

#include <cstdlib>
#include <cstring>

#include "ozz/animation/offline/additive_animation_builder.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/raw_track.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/track_builder.h"
#include "ozz/animation/offline/track_optimizer.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/track.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include "ozz/base/log.h"

#include "ozz/options/options.h"

// Declares command line options.
OZZ_OPTIONS_DECLARE_STRING(file, "Specifies input file", "", true)
OZZ_OPTIONS_DECLARE_STRING(skeleton,
                           "Specifies ozz skeleton (raw or runtime) input file",
                           "", true)
/*
struct ImportAnimation {
  float sampling_rate = 0.f;
  bool raw = false;
  bool additive = false;
  bool optimize = true;
  float translation_tolerance =
      ozz::animation::offline::AnimationOptimizer().translation_tolerance;
  float rotation_tolerance =
      ozz::animation::offline::AnimationOptimizer().rotation_tolerance;
  float scale_tolerance =
      ozz::animation::offline::AnimationOptimizer().scale_tolerance;
  float hierarchical_tolerance =
      ozz::animation::offline::AnimationOptimizer().hierarchical_tolerance;
};

struct ImportTrack {
  ozz::String::Std node;
  ozz::String::Std name;
};*/

// animation option can have 0 or 1 "Asterisk" (*) character to specify which
// part of the filename should be replaced by the animation name (imported from
// the DCC source file).
// If no * is given, only a single animation is imported from the input file.
static bool ValidateAnimation(const ozz::options::Option& _option,
                              int /*_argc*/) {
  const ozz::options::StringOption& option =
      static_cast<const ozz::options::StringOption&>(_option);
  const char* file = option.value();
  // Count the number of '*'.
  size_t asterisks = 0;
  for (; file[asterisks] != 0; file[asterisks] == '*' ? ++asterisks : *++file)
    ;
  if (asterisks > 1) {
    ozz::log::Err()
        << "Invalid file option. There should be 0 or 1 \'*\' character."
        << std::endl;
  }
  return asterisks <= 1;
}
OZZ_OPTIONS_DECLARE_STRING_FN(
    animation,
    "Specifies ozz animation output file(s). When importing multiple "
    "animations, "
    "use a \'*\' character to specify which part of the filename should be "
    "replaced by the animation name.",
    "", true, &ValidateAnimation)

OZZ_OPTIONS_DECLARE_BOOL(optimize, "Activate keyframes optimization stage.",
                         true, false)

OZZ_OPTIONS_DECLARE_FLOAT(
    rotation, "Optimizer rotation tolerance in degrees",
    ozz::animation::offline::AnimationOptimizer().rotation_tolerance, false)
OZZ_OPTIONS_DECLARE_FLOAT(
    translation, "Optimizer translation tolerance in meters",
    ozz::animation::offline::AnimationOptimizer().translation_tolerance, false)
OZZ_OPTIONS_DECLARE_FLOAT(
    scale, "Optimizer scale tolerance in percents",
    ozz::animation::offline::AnimationOptimizer().scale_tolerance, false)
OZZ_OPTIONS_DECLARE_FLOAT(
    hierarchical, "Optimizer hierarchical tolerance in meters",
    ozz::animation::offline::AnimationOptimizer().hierarchical_tolerance, false)

OZZ_OPTIONS_DECLARE_BOOL(
    additive,
    "Creates a delta animation that can be used for additive blending.", false,
    false)

static bool ValidateEndianness(const ozz::options::Option& _option,
                               int /*_argc*/) {
  const ozz::options::StringOption& option =
      static_cast<const ozz::options::StringOption&>(_option);
  bool valid = std::strcmp(option.value(), "native") == 0 ||
               std::strcmp(option.value(), "little") == 0 ||
               std::strcmp(option.value(), "big") == 0;
  if (!valid) {
    ozz::log::Err() << "Invalid endianess option." << std::endl;
  }
  return valid;
}

OZZ_OPTIONS_DECLARE_STRING_FN(
    endian,
    "Selects output endianness mode. Can be \"native\" (same as current "
    "platform), \"little\" or \"big\".",
    "native", false, &ValidateEndianness)

ozz::Endianness Endianness() {
  // Initializes output endianness from options.
  ozz::Endianness endianness = ozz::GetNativeEndianness();
  if (std::strcmp(OPTIONS_endian, "little")) {
    endianness = ozz::kLittleEndian;
  } else if (std::strcmp(OPTIONS_endian, "big")) {
    endianness = ozz::kBigEndian;
  }
  ozz::log::LogV() << (endianness == ozz::kLittleEndian ? "Little" : "Big")
                   << " Endian output binary format selected." << std::endl;
  return endianness;
}

static bool ValidateLogLevel(const ozz::options::Option& _option,
                             int /*_argc*/) {
  const ozz::options::StringOption& option =
      static_cast<const ozz::options::StringOption&>(_option);
  bool valid = std::strcmp(option.value(), "verbose") == 0 ||
               std::strcmp(option.value(), "standard") == 0 ||
               std::strcmp(option.value(), "silent") == 0;
  if (!valid) {
    ozz::log::Err() << "Invalid log level option." << std::endl;
  }
  return valid;
}

OZZ_OPTIONS_DECLARE_STRING_FN(
    log_level,
    "Selects log level. Can be \"silent\", \"standard\" or \"verbose\".",
    "standard", false, &ValidateLogLevel)

static bool ValidateSamplingRate(const ozz::options::Option& _option,
                                 int /*_argc*/) {
  const ozz::options::FloatOption& option =
      static_cast<const ozz::options::FloatOption&>(_option);
  bool valid = option.value() >= 0.f;
  if (!valid) {
    ozz::log::Err() << "Invalid sampling rate option (must be >= 0)."
                    << std::endl;
  }
  return valid;
}

OZZ_OPTIONS_DECLARE_FLOAT_FN(sampling_rate,
                             "Selects animation sampling rate in hertz. Set a "
                             "value = 0 to use imported scene frame rate.",
                             0.f, false, &ValidateSamplingRate)

OZZ_OPTIONS_DECLARE_BOOL(raw,
                         "Outputs raw animation, instead of runtime animation.",
                         false, false)

OZZ_OPTIONS_DECLARE_STRING(tracks, "Specifies tracks to import", "", false);

namespace ozz {
namespace animation {
namespace offline {

namespace {
void DisplaysOptimizationstatistics(const RawAnimation& _non_optimized,
                                    const RawAnimation& _optimized) {
  size_t opt_translations = 0, opt_rotations = 0, opt_scales = 0;
  for (size_t i = 0; i < _optimized.tracks.size(); ++i) {
    const RawAnimation::JointTrack& track = _optimized.tracks[i];
    opt_translations += track.translations.size();
    opt_rotations += track.rotations.size();
    opt_scales += track.scales.size();
  }
  size_t non_opt_translations = 0, non_opt_rotations = 0, non_opt_scales = 0;
  for (size_t i = 0; i < _non_optimized.tracks.size(); ++i) {
    const RawAnimation::JointTrack& track = _non_optimized.tracks[i];
    non_opt_translations += track.translations.size();
    non_opt_rotations += track.rotations.size();
    non_opt_scales += track.scales.size();
  }

  // Computes optimization ratios.
  float translation_ratio =
      non_opt_translations != 0
          ? 100.f * (non_opt_translations - opt_translations) /
                non_opt_translations
          : 0;
  float rotation_ratio =
      non_opt_rotations != 0
          ? 100.f * (non_opt_rotations - opt_rotations) / non_opt_rotations
          : 0;
  float scale_ratio =
      non_opt_scales != 0
          ? 100.f * (non_opt_scales - opt_scales) / non_opt_scales
          : 0;

  ozz::log::LogV() << "Optimization stage results:" << std::endl;
  ozz::log::LogV() << " - Translations key frames optimization: "
                   << translation_ratio << "%" << std::endl;
  ozz::log::LogV() << " - Rotations key frames optimization: " << rotation_ratio
                   << "%" << std::endl;
  ozz::log::LogV() << " - Scaling key frames optimization: " << scale_ratio
                   << "%" << std::endl;
}

ozz::animation::Skeleton* ImportSkeleton() {
  // Reads the skeleton from the binary ozz stream.
  ozz::animation::Skeleton* skeleton = NULL;
  {
    ozz::log::LogV() << "Opens input skeleton ozz binary file: "
                     << OPTIONS_skeleton << std::endl;
    ozz::io::File file(OPTIONS_skeleton, "rb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open input skeleton ozz binary file: "
                      << OPTIONS_skeleton << std::endl;
      return NULL;
    }
    ozz::io::IArchive archive(&file);

    // File could contain a RawSkeleton or a Skeleton.
    if (archive.TestTag<ozz::animation::offline::RawSkeleton>()) {
      ozz::log::LogV() << "Reading RawSkeleton from file." << std::endl;

      // Reading the skeleton cannot file.
      ozz::animation::offline::RawSkeleton raw_skeleton;
      archive >> raw_skeleton;

      // Builds runtime skeleton.
      ozz::log::LogV() << "Builds runtime skeleton." << std::endl;
      ozz::animation::offline::SkeletonBuilder builder;
      skeleton = builder(raw_skeleton);
      if (!skeleton) {
        ozz::log::Err() << "Failed to build runtime skeleton." << std::endl;
        return NULL;
      }
    } else if (archive.TestTag<ozz::animation::Skeleton>()) {
      // Reads input archive to the runtime skeleton.
      // This operation cannot fail.
      skeleton =
          ozz::memory::default_allocator()->New<ozz::animation::Skeleton>();
      archive >> *skeleton;
    } else {
      ozz::log::Err() << "Failed to read input skeleton from binary file: "
                      << OPTIONS_skeleton << std::endl;
      return NULL;
    }
  }
  return skeleton;
}

bool OutputSingleAnimation() {
  return strchr(OPTIONS_animation.value(), '*') == NULL;
}

ozz::String::Std BuildFilename(const char* _filename, const char* _animation) {
  ozz::String::Std output(_filename);
  const size_t asterisk = output.find('*');
  if (asterisk != std::string::npos) {
    output.replace(asterisk, 1, _animation);
  }
  return output;
}

bool Export(const ozz::animation::offline::RawAnimation _raw_animation,
            const ozz::animation::Skeleton& _skeleton) {
  // Raw animation to build and output.
  ozz::animation::offline::RawAnimation raw_animation;

  // Make delta animation if requested.
  if (OPTIONS_additive) {
    ozz::log::LogV() << "Makes additive animation." << std::endl;
    ozz::animation::offline::AdditiveAnimationBuilder additive_builder;
    RawAnimation raw_additive;
    if (!additive_builder(_raw_animation, &raw_additive)) {
      ozz::log::Err() << "Failed to make additive animation." << std::endl;
      return false;
    }
    // Copy animation.
    raw_animation = raw_additive;
  } else {
    raw_animation = _raw_animation;
  }

  // Optimizes animation if option is enabled.
  if (OPTIONS_optimize) {
    ozz::log::LogV() << "Optimizing animation." << std::endl;
    ozz::animation::offline::AnimationOptimizer optimizer;
    optimizer.rotation_tolerance = OPTIONS_rotation;
    optimizer.translation_tolerance = OPTIONS_translation;
    optimizer.scale_tolerance = OPTIONS_scale;
    optimizer.hierarchical_tolerance = OPTIONS_hierarchical;
    ozz::animation::offline::RawAnimation raw_optimized_animation;
    if (!optimizer(raw_animation, _skeleton, &raw_optimized_animation)) {
      ozz::log::Err() << "Failed to optimize animation." << std::endl;
      return false;
    }

    // Displays optimization statistics.
    DisplaysOptimizationstatistics(raw_animation, raw_optimized_animation);

    // Brings data back to the raw animation.
    raw_animation = raw_optimized_animation;
  }

  // Builds runtime animation.
  ozz::animation::Animation* animation = NULL;
  if (!OPTIONS_raw) {
    ozz::log::LogV() << "Builds runtime animation." << std::endl;
    ozz::animation::offline::AnimationBuilder builder;
    animation = builder(raw_animation);
    if (!animation) {
      ozz::log::Err() << "Failed to build runtime animation." << std::endl;
      return false;
    }
  }

  {
    // Prepares output stream. File is a RAII so it will close automatically at
    // the end of this scope.
    // Once the file is opened, nothing should fail as it would leave an invalid
    // file on the disk.

    // Builds output filename.
    const ozz::String::Std filename =
        BuildFilename(OPTIONS_animation, _raw_animation.name.c_str());

    ozz::log::LogV() << "Opens output file: " << filename << std::endl;
    ozz::io::File file(filename.c_str(), "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: " << filename
                      << std::endl;
      ozz::memory::default_allocator()->Delete(animation);
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, Endianness());

    // Fills output archive with the animation.
    if (OPTIONS_raw) {
      ozz::log::LogV() << "Outputs RawAnimation to binary archive."
                       << std::endl;
      archive << raw_animation;
    } else {
      ozz::log::LogV() << "Outputs Animation to binary archive." << std::endl;
      archive << *animation;
    }
  }

  ozz::log::LogV() << "Animation binary archive successfully outputted."
                   << std::endl;

  // Delete local objects.
  ozz::memory::default_allocator()->Delete(animation);

  return true;
}

bool Export(const ozz::animation::offline::RawFloatTrack& _raw_track) {
  // Raw track to build and output.
  ozz::animation::offline::RawFloatTrack raw_track;

  // Optimizes track if option is enabled.
  if (OPTIONS_optimize) {
    ozz::log::LogV() << "Optimizing track." << std::endl;
    ozz::animation::offline::TrackOptimizer optimizer;
    // optimizer.rotation_tolerance = OPTIONS_rotation;
    ozz::animation::offline::RawFloatTrack raw_optimized_track;
    if (!optimizer(_raw_track, &raw_optimized_track)) {
      ozz::log::Err() << "Failed to optimize track." << std::endl;
      return false;
    }

    // Displays optimization statistics.
    // DisplaysOptimizationstatistics(raw_animation, raw_optimized_animation);

    // Brings data back to the raw track.
    raw_track = raw_optimized_track;
  } else {
    raw_track = _raw_track;
  }

  // Builds runtime track.
  ozz::animation::FloatTrack* track = NULL;
  if (!OPTIONS_raw) {
    ozz::log::LogV() << "Builds runtime track." << std::endl;
    ozz::animation::offline::TrackBuilder builder;
    track = builder(raw_track);
    if (!track) {
      ozz::log::Err() << "Failed to build runtime track." << std::endl;
      return false;
    }
  }

  {
    // Prepares output stream. File is a RAII so it will close automatically at
    // the end of this scope.
    // Once the file is opened, nothing should fail as it would leave an invalid
    // file on the disk.

    // Builds output filename.
    const ozz::String::Std filename = "temp_track";
    //        BuildFilename(OPTIONS_animation, _raw_animation.name.c_str());

    ozz::log::LogV() << "Opens output file: " << filename << std::endl;
    ozz::io::File file(filename.c_str(), "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: " << filename
                      << std::endl;
      ozz::memory::default_allocator()->Delete(track);
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, Endianness());

    // Fills output archive with the track.
    if (OPTIONS_raw) {
      ozz::log::LogV() << "Outputs RawTrack to binary archive." << std::endl;
      archive << raw_track;
    } else {
      ozz::log::LogV() << "Outputs Track to binary archive." << std::endl;
      archive << *track;
    }
  }

  ozz::log::LogV() << "Track binary archive successfully outputted."
                   << std::endl;

  // Delete local objects.
  ozz::memory::default_allocator()->Delete(track);

  return true;
}
}  // namespace

int AnimationConverter::operator()(int _argc, const char** _argv) {
  // Parses arguments.
  ozz::options::ParseResult parse_result = ozz::options::ParseCommandLine(
      _argc, _argv, "1.1",
      "Imports a animation from a file and converts it to ozz binary raw or "
      "runtime animation format");
  if (parse_result != ozz::options::kSuccess) {
    return parse_result == ozz::options::kExitSuccess ? EXIT_SUCCESS
                                                      : EXIT_FAILURE;
  }

  // Initializes log level from options.
  ozz::log::Level log_level = ozz::log::GetLevel();
  if (std::strcmp(OPTIONS_log_level, "silent") == 0) {
    log_level = ozz::log::Silent;
  } else if (std::strcmp(OPTIONS_log_level, "standard") == 0) {
    log_level = ozz::log::Standard;
  } else if (std::strcmp(OPTIONS_log_level, "verbose") == 0) {
    log_level = ozz::log::Verbose;
  }
  ozz::log::SetLevel(log_level);

  // Ensures file to import actually exist.
  if (!ozz::io::File::Exist(OPTIONS_file)) {
    ozz::log::Err() << "File \"" << OPTIONS_file << "\" doesn't exist."
                    << std::endl;
    return EXIT_FAILURE;
  }

  // Imports animations from the document.
  ozz::log::Log() << "Importing file \"" << OPTIONS_file << "\"" << std::endl;
  if (!Load(OPTIONS_file)) {
    ozz::log::Err() << "Failed to import file \"" << OPTIONS_file << "\""
                    << std::endl;
    return EXIT_FAILURE;
  }

  // Import skeleton instance.
  ozz::animation::Skeleton* skeleton = ImportSkeleton();
  if (!skeleton) {
    return EXIT_FAILURE;
  }

  // Get all available animation names.
  AnimationNames animation_names = GetAnimationNames();

  // Are there animations available
  if (animation_names.empty()) {
    ozz::log::Err() << "No animation found." << std::endl;
    return EXIT_FAILURE;
  }

  // Check whether multiple animation are supported.
  if (OutputSingleAnimation() && animation_names.size() > 1) {
    ozz::log::Log() << animation_names.size()
                    << " animations found. Only the first one ("
                    << animation_names[0] << ") will be exported." << std::endl;

    // Removes all unhandled animations.
    animation_names.resize(1);
  }

  // Iterates all imported animations, build and output them.
  bool success = true;
  for (size_t i = 0; success && i < animation_names.size(); ++i) {
    RawAnimation animation;
    success = Import(animation_names[i].c_str(), *skeleton,
                     OPTIONS_sampling_rate, &animation);
    if (success) {
      success &= Export(animation, *skeleton);
    } else {
      ozz::log::Err() << "Failed to import animation \"" << animation_names[0]
                      << "\"" << std::endl;
    }
  }

  // Iterates all tracks, build and output them.
  const char* track_defintion = OPTIONS_tracks;
  if (track_defintion[0] != 0) {
    for (size_t i = 0; success && i < animation_names.size(); ++i) {
      const char* separator = strchr(track_defintion, ':');
      ozz::String::Std node_name(track_defintion, separator);
      ozz::String::Std track_name(++separator);
      RawFloatTrack track;
      success &= Import(animation_names[i].c_str(), node_name.c_str(),
                        track_name.c_str(), OPTIONS_sampling_rate, &track);
      if (success) {
        success &= Export(track);
      } else {
        ozz::log::Err() << "Failed to import track \"" << node_name << ":"
                        << track_name << "\"" << std::endl;
      }
    }
  }

  ozz::memory::default_allocator()->Delete(skeleton);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
