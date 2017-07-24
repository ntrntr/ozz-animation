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

#ifndef OZZ_ANIMATION_OFFLINE_FBX_FBX_ANIMATION_H_
#define OZZ_ANIMATION_OFFLINE_FBX_FBX_ANIMATION_H_

#include "ozz/animation/offline/fbx/fbx.h"

#include "ozz/base/containers/string.h"
#include "ozz/base/containers/vector.h"

namespace ozz {
namespace animation {

class Skeleton;

namespace offline {

struct RawAnimation;
struct RawFloatTrack;

namespace fbx {

typedef ozz::Vector<ozz::String::Std>::Std AnimationNames;
AnimationNames GetAnimationNames(FbxSceneLoader& _scene_loader);

bool ExtractAnimation(const char* _animation_name,
                      FbxSceneLoader& _scene_loader, const Skeleton& _skeleton,
                      float _sampling_rate, RawAnimation* _animation);

bool ExtractTrack(const char* _animation_name, const char* _node_name,
                  const char* _track_name, FbxSceneLoader& _scene_loader,
                  float _sampling_rate, RawFloatTrack* _track);

}  // namespace fbx
}  // namespace offline
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_ANIMATION_OFFLINE_FBX_FBX_ANIMATION_H_