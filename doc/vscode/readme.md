# Developing Inkscape with Visual Studio Code on Windows

1. Follow these sections of [the tutorial for compiling Inkscape on Windows](../building/windows.md):

    1. _Compiling Inkscape on Windows_ **or** _Installing Build Dependencies Manually_

    0. _Obtaining Inkscape Source_
    
    Note: if you install MSYS2 to a different directory than the default (`C:/msys64`), you will have to correct the path in the JSON files below.

0. In your clone of the repository (called `master` in the compilation tutorial), make a new folder named: `.vscode`

0. Copy [c_cpp_properties.json](./c_cpp_properties.json), [launch.json](./launch.json), and [tasks.json](./tasks.json) from `doc/vscode` to that folder.

0. Install [Visual Studio Code](https://code.visualstudio.com/) if you haven't.

0. Launch VS Code and install [the official C/C++ extension pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack) or at least [the main extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). This is easily done via the extensions panel (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>X</kbd>).

0. Open the command palette (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>P</kbd>) and enter `> Open User Settings (JSON)`.

0. In the file that opens, add the following. This will allow you to launch the relevant MSYS2 terminals from within VS Code (<kbd>Ctrl</kbd> + <kbd>\`</kbd>) for convenience. If `terminal.integrated.profiles.windows` already exists, just add the items `UCRT` and `MSYS` to it. Note the presence of commas between adjacent list items.
    ```json
    "terminal.integrated.profiles.windows": {
        "UCRT": {
            "path": "cmd.exe",
            "args": [ "/c", "C:/msys64/msys2_shell.cmd -defterm -here -no-start -ucrt64" ],
            "overrideName": true
        },
        "MSYS": {
            "path": "cmd.exe",
            "args": [ "/c", "C:/msys64/msys2_shell.cmd -defterm -here -no-start -msys" ],
            "overrideName": true
        }
    }
    ```

0. Save and close `settings.json`.

0. Open the folder with your clone of the Inkscape repository. You can open a folder with `File > Open Folder...`.

0. To build Inkscape, I recommend trying via the terminal first to make it easier to troubleshoot. You can do that in VS Code: in the command palette, enter `> Create New Terminal (With Profile)` and choose `UCRT`.

    At this point you can follow [the commands from the compilation tutorial](../building/windows.md#building-inkscape-with-msys2), but for debugging and IntelliSense you will want to include some CMake flags:

    ```sh
    cd build

    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF -G Ninja ..

    ninja install
    ```

    Here is what they mean:

    - `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` tells CMake to output `build/compile_commands.json`. [c_cpp_properties.json](./c_cpp_properties.json) tells IntelliSense to use this to understand the code, preventing a lot of false errors.

    - `-DCMAKE_BUILD_TYPE=Debug` is for debug symbols.

    - `-DBUILD_SHARED_LIBS=OFF` avoids trying to build with [too many debug symbols](https://stackoverflow.com/questions/47135973/error-export-ordinal-too-large-104116).

0. To make the build process more convenient, [tasks.json](./tasks.json) defines tasks called `CMake` and `Ninja Install` that you can run via the command palette by entering `task ` followed by the name. These require the `build` folder to exist.

    In addition, <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>B</kbd> is bound to the "default build task", which will be `Ninja Install` by default.
    
0. [launch.json](./launch.json) defines a _debug configuration_ that you can use to debug Inkscape using VS Code. Open the _Run and Debug_ panel (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>D</kbd>) and choose `(gdb) Launch`.
    
    Then, when you press the green play button (_Start Debugging_, <kbd>F5</kbd>), the `Ninja Install` task will be run to build Inkscape, followed by the resulting Inkscape executable, and the debugger will be attached.
