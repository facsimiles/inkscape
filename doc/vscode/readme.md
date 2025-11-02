# Developing Inkscape with Visual Studio Code on Windows

1. Follow these sections of [the tutorial for compiling Inkscape on Windows](../building/windows.md):

    1. _Compiling Inkscape on Windows_ **or** _Installing Build Dependencies Manually_

    0. _Obtaining Inkscape Source_
    
    Note: if you install MSYS2 to a different directory than the default (`C:/msys64`), you will have to correct the path in the JSON files below.

0. In your clone of the repository (called `master` in the compilation tutorial), make a new folder named: `.vscode`

0. Copy all `.json` files from `doc/vscode` to that folder. This is the Inkscape-specific configuration of VSCode for the repository. Details about these files (c_cpp_properties.json, settings.json, tasks.json, launch.json) will be explained later.

0. Install [Visual Studio Code](https://code.visualstudio.com/) if you haven't.

0. Launch VSCode.

0. Open the folder with your clone of the Inkscape repository. You can open a folder with `File > Open Folder...`.

0. When asked "Do you want to install the recommended installations for this repository", accept by clicking "Install". You can also do this later by installing [the official C/C++ extension pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) or at least [the main extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) via the extensions panel (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>X</kbd>).

0. [settings.json](./settings.json) defines terminal profiles that you need for running Inkscape build commands inside the VS Code terminal: UCRT (normal) and MSYS2 (special cases only). To launch the relevant MSYS2 terminals press <kbd>Ctrl</kbd> + <kbd>\`</kbd>.

0. To build Inkscape, I recommend trying via the terminal first to make it easier to troubleshoot. You can do that in VS Code: in the command palette, enter `> Create New Terminal (With Profile)` and choose `UCRT`.

    At this point you can follow [the commands from the compilation tutorial](../building/windows.md#building-inkscape-with-msys2).

    (Footnote: The build flag `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` in the standard build commands tells CMake to output `build/compile_commands.json`. [c_cpp_properties.json](./c_cpp_properties.json) tells IntelliSense to use this to understand the code, preventing a lot of false errors.)

0. To make the build process more convenient, [tasks.json](./tasks.json) defines tasks called `CMake` and `Ninja Install` that you can run via the command palette by entering `task ` followed by the name. These require the `build` folder to exist.

    In addition, <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>B</kbd> is bound to the "default build task", which will be `Ninja Install` by default.
    
0. [launch.json](./launch.json) defines a _debug configuration_ that you can use to debug Inkscape using VS Code. Open the _Run and Debug_ panel (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>D</kbd>) and choose `(gdb) Launch`.
    
    Then, when you press the green play button (_Start Debugging_, <kbd>F5</kbd>), the `Ninja Install` task will be run to build Inkscape, followed by the resulting Inkscape executable, and the debugger will be attached.
