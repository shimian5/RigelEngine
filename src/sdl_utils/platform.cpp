/* Copyright (C) 2021, Nikolai Wuttke. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform.hpp"

#include <SDL_video.h>

#ifdef __ANDROID__
  #include <SDL_log.h>
  #include <SDL_system.h>
  #include <jni.h>
#endif

#include <string_view>


namespace rigel::sdl_utils
{

bool isRunningInDesktopEnvironment()
{
  const auto sdlVideoDriver = std::string_view(SDL_GetCurrentVideoDriver());

  // clang-format off
  return
    sdlVideoDriver == "cocoa" ||
    sdlVideoDriver == "wayland" ||
    sdlVideoDriver == "windows" ||
    sdlVideoDriver == "x11";
  // clang-format on
}

#ifdef __ANDROID__
void openAndroidGameDataSetup()
{
  auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !activity)
  {
    SDL_Log("Unable to open game-data setup");
    return;
  }

  auto activityClass = env->GetObjectClass(activity);
  auto method = activityClass
    ? env->GetMethodID(activityClass, "openGameDataSetup", "()V")
    : nullptr;
  if (method)
    env->CallVoidMethod(activity, method);

  if (env->ExceptionCheck())
  {
    env->ExceptionClear();
    SDL_Log("Unable to open game-data setup");
  }
  if (activityClass)
    env->DeleteLocalRef(activityClass);
  env->DeleteLocalRef(activity);
}
#endif

} // namespace rigel::sdl_utils
