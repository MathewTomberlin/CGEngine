#CG Engine

THIS SOFTWARE IS IN ALPHA DEVELOPMENT AND IS RELEASED AS-IS. FEATURES ARE LIKELY TO CHANGE WITHOUT NOTICE.

This C++ interaction library allows you to create real-time, scriptable visual interfaces and games using composable Bodies, Behaviors, Scripts, and Timers.

This project template is built on the SFML CMake GitHub template. Instructions for using that template are below and many should be the same for this project template, but they have not been reviewed or confirmed.

#CG Engine Details
CG Engine
- C++ 2D/3D scene graph scripting engine
- Libraries: SFML, OpenGL, CMake, Assimp

Engine Systems
- Engine defines global extern objects (World, Renderer, AssetManager, etc)
- AssetManager has a Type-to-UniqueDomain map of asset pointer (IResource) to unique id within that domain
- Assets managed by AssetManager should be referenced by id wherever possible
- Renderer handles OpenGL to render Meshes and/or SFML draw functions to draw SFML entities
- World holds the root Body of the scene graph and other world state
- InputMap has a InputCondition-to-UniqueDomain map of Script to unique id.
- InputMap gathers input and calls domains when input matches InputCondition for the domain

Engine Classes
- Body holds entities, like Mesh or SFML's Shape, Sprite, and Text
- Body holds ScriptMap and can call scripts by domain string. Start, Update, and Delete domains called by engine.
- Body holds vector of Behaviors. Behavior Start, Update, and Delete called with Body.
- ScriptMap has a string-to-UniqueDomain map of Script to unique id.
- Script holds ScriptEvent function pointer that takes ScArgs (script Script*, caller Body*, behave Behavior*)
- Script holds input & output DataMaps for state and input/output
- Actuators are Scripts with caller and behavior reference that can be called from anywhere (as InputMap does)
- Behavior holds a ScriptMap and input, output, & process DataMaps for state and input/output
- Behavior does not have entity, like Body
- Mesh entities are Drawable and Transformable OpenGL 3D entities
- Models are compositions of Bodies with Meshes and may be imported via Assimp
- Shaders are loaded from file, to allow developers to define custom shaders
- Programs are loaded from shaders
- Materials use a string-to-any value map of parameters to allow developers to define custom materials

Keep these implementation details in mind when analyzing classes and providing suggestions. If you notice code that does not support the above implementation details, raise a warning. Always use logic and reasoning when suggesting new code and be clear where code has changed.

# CMake SFML Project Template

This repository template should allow for a fast and hassle-free kick start of your next SFML project using CMake.
Thanks to [GitHub's nature of templates](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-repository-from-a-template), you can fork this repository without inheriting its Git history.

The template starts out very basic, but might receive additional features over time:

- Basic CMake script to build your project and link SFML on any operating system


## How to Use

1. Install [Git](https://git-scm.com/downloads) and [CMake](https://cmake.org/download/). Use your system's package manager if available.
2. Follow [GitHub's instructions](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-repository-from-a-template) for how to use their project template feature to create your own project. If you don't want to use GitHub, see the section below.
3. Clone your new GitHub repo and open the repo in your text editor of choice.
4. Open [CMakeLists.txt](CMakeLists.txt). Rename the project and the target name of the executable to whatever name you want. Make sure to change all occurrences.
5. If you want to add or remove any .cpp files, change the source files listed in the `add_executable` call in CMakeLists.txt to match the source files your project requires. If you plan on keeping the default main.cpp file then no changes are required.
6. If your code uses the Audio or Network modules then add `sfml-audio` or `sfml-network` to the `target_link_libraries` call alongside the existing `sfml-graphics` library that is being linked.
7. If you use Linux, install SFML's dependencies using your system package manager. On Ubuntu and other Debian-based distributions you can use the following commands:
   ```
   sudo apt update
   sudo apt install \
       libxrandr-dev \
       libxcursor-dev \
       libxi-dev \
       libudev-dev \
       libfreetype-dev \
       libflac-dev \
       libvorbis-dev \
       libgl1-mesa-dev \
       libegl1-mesa-dev \
       libfreetype-dev
   ```
8. Configure and build your project. Most popular IDEs support CMake projects with very little effort on your part.

   - [VS Code](https://code.visualstudio.com) via the [CMake extension](https://code.visualstudio.com/docs/cpp/cmake-linux)
   - [Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170)
   - [CLion](https://www.jetbrains.com/clion/features/cmake-support.html)
   - [Qt Creator](https://doc.qt.io/qtcreator/creator-project-cmake.html)

   Using CMake from the command line is straightforward as well.
   Be sure to run these commands in the root directory of the project you just created.

   ```
   cmake -B build
   cmake --build build
   ```

9. Enjoy!

## Upgrading SFML

SFML is found via CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module.
FetchContent automatically downloads SFML from GitHub and builds it alongside your own code.
Beyond the convenience of not having to install SFML yourself, this ensures ABI compatibility and simplifies things like specifying static versus shared libraries.

Modifying what version of SFML you want is as easy as changing the `GIT_TAG` argument.
Currently it uses SFML 3 via the `3.0.0` tag.

## But I want to...

Modify CMake options by adding them as configuration parameters (with a `-D` flag) or by modifying the contents of CMakeCache.txt and rebuilding.

### Not use GitHub

You can use this project without a GitHub account by [downloading the contents](https://github.com/SFML/cmake-sfml-project/archive/refs/heads/master.zip) of the repository as a ZIP archive and unpacking it locally.
This approach also avoids using Git entirely if you would prefer to not do that.

### Change Compilers

See the variety of [`CMAKE_<LANG>_COMPILER`](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER.html) options.
In particular you'll want to modify `CMAKE_CXX_COMPILER` to point to the C++ compiler you wish to use.

### Change Compiler Optimizations

CMake abstracts away specific optimizer flags through the [`CMAKE_BUILD_TYPE`](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html) option.
By default this project recommends `Release` builds which enable optimizations.
Other build types include `Debug` builds which enable debug symbols but disable optimizations.
If you're using a multi-configuration generator (as is often the case on Windows), you can modify the [`CMAKE_CONFIGURATION_TYPES`](https://cmake.org/cmake/help/latest/variable/CMAKE_CONFIGURATION_TYPES.html#variable:CMAKE_CONFIGURATION_TYPES) option.

### Change Generators

While CMake will attempt to pick a suitable default generator, some systems offer a number of generators to choose from.
Ubuntu, for example, offers Makefiles and Ninja as two potential options.
For a list of generators, click [here](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).
To modify the generator you're using you must reconfigure your project providing a `-G` flag with a value corresponding to the generator you want.
You can't simply modify an entry in the CMakeCache.txt file unlike the above options.
Then you may rebuild your project with this new generator.

## More Reading

Here are some useful resources if you want to learn more about CMake:

- [Official CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/)
- [How to Use CMake Without the Agonizing Pain - Part 1](https://alexreinking.com/blog/how-to-use-cmake-without-the-agonizing-pain-part-1.html)
- [How to Use CMake Without the Agonizing Pain - Part 2](https://alexreinking.com/blog/how-to-use-cmake-without-the-agonizing-pain-part-2.html)
- [Better CMake YouTube series by Jefferon Amstutz](https://www.youtube.com/playlist?list=PL8i3OhJb4FNV10aIZ8oF0AA46HgA2ed8g)

## License

The source code is dual licensed under Public Domain and MIT -- choose whichever you prefer.
