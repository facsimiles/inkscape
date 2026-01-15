# Development environment VM for Inkscape on Windows 11

*Experimental. Only recommended for advaced developers.* This page explains how to *automatically* set up a *reproducible* Windows VM for developing Inkscape, using [Vagrant](https://developer.hashicorp.com/vagrant/install).

The automatic setup includes:
- Creating a new VM
- Installing Windows
- Running `buildtools/windows-installFullDevEnv-clickHere.bat` to set up the [development environment for Inkscape on Windows](../../doc/building/windows.md), which includes:
- Installing all build dependencies
- Compiling Inkscape and running it
- Setting up VSCode and opening it

The benefit is that you can destroy and re-create the environment with just one command. This is especially helpful for reproducing bugs in the build instructions. The downside is that you need extra tools and add another layer of complexity.

As a manual alternative, simply follow the instructions for [Compiling Inkscape on Windows](../../doc/building/windows.md) by hand.

# Basic Information

## Requirements
- Windows or Linux Host PC with at least 16 GB RAM, 100 GB free disk space, CPU new enough to run latest Windows
- It may work with less.

## Caution
- You agree to the license terms of the used dependencies, especially of Windows, VirtualBox and Vagrant.
- The VM is based on a Vagrant image from a third party. It seems that Vagrant does not guarantee isolation of the host from malicious Vagrant images. If you are paranoid you could put Vagrant itself inside a VM (nested virtualization).

## Hyper-V vs. VirtualBox / Limitations for Windows Hosts
You can use VirtualBox or Microsoft Hyper-V as virtualization tool. The following instructions explain both.

*If your (host) PC is running Linux:* Use VirtualBox. Note that VirtualBox does not work together with KVM-based virtualization. The instructions here do not yet support KVM.

*If your (host) PC is running Windows:*

Using VirtualBox is easier but typically much slower than Hyper-V.

VirtualBox conflicts with the Microsoft Hyper-V virtualization tool that is part of Windows. The "slow mode" of VirtualBox is shown by a green turtle icon in the bottom of the VirtualBox window. Disabling Hyper-V is possible but [quite complicated with newer versions of Windows](https://superuser.com/questions/1778061/green-turtle-snail-mode-slow-performance-indicator-on-virtualbox-vm) and can disable other Windows features such as WSL.


Hyper-V is more difficult because it is not well supported by Vagrant, the tool that we use to automate setting up the VM. The loosely related project ["chocolatey-test-environment"](https://github.com/chocolatey-community/chocolatey-test-environment/blob/ba08f73a1a3c07974ff8872ba0c0ad04fb4acb34/ReadMe.md#using-hyper-v-instead-of-virtualbox) can be a starting point for research.

## Initial setup
0. Install [VirtualBox](https://www.virtualbox.org/wiki/Downloads) or [Hyper-V](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/get-started/Install-Hyper-V?tabs=powershell&pivots=windows) - choose as described above. 
0. Install [Vagrant](https://developer.hashicorp.com/vagrant/install)
0. Get the `buildtools/windows-dev-vm` folder of the Inkscape repository
0. Open a terminal in that folder
   - For Hyper-V: Use administrative Terminal (right click on the start menu and select "Terminal (Administrator)").
0. Set up the dev VM:
   - For VirtualBox: Set up the dev VM with `vagrant up` (VirtualBox). Progress output will mostly be shown in the terminal. Some messages will also be shown in the graphical VM itself. A VirtualBox window will open automatically and show the screen of the VM.
   - For Hyper-V: Similar to VirtualBox, but:
     - Start the VM with `vagrant up --provider hyperv`.
     - In the start menu, start "Hyper-V manager" and open the newly created VM to see its screen.)
0. If everything is successful, the command will finish and show "Done." as last line.
0. VSCode will open with the Inkscape project. Please accept "Yes, I trust the authors" (of the Inkscape folder) when asked by VSCode.
0. The compiled Inkscape will open.

## Usage after setup
(If you use Hyper-V, add ` --provider hyperv` to all `vagrant up` commands in the following explanations.)

### Start
- If the VM is not running, use `vagrant up` to start it. Log in with password "vagrant".
- The Inkscape git repository is in `C:/Users/Vagrant/inkscape`. Note that this folder is called `master` in the Inkscape developer documentation.
- VSCode: Open that folder.
- or: Command line: Open "UCRT64" from the start menu and `cd /c/Users/Vagrant/inkscape`
- The compiled Inkscape EXE is in `C:/Users/Vagrant/inkscape/install_dir/bin/inkscape.exe`
- To connect into the VM (like SSH): `vagrant ssh`, then `powershell`

### Reset
- Re-trigger the setup with `vagrant up; vagrant provision`. This is a "quick-and-dirty" approach --- it tries to continue from the previous state but will not always do that reliably.
- To recreate the VM from scratch, deleting all data inside the VM (!!!): `vagrant destroy; vagrant up`. This is cleanly reproducible. **Caution**: Any changes to the Inkscape sourcecode that you did inside the VM will be lost. Double-check that you committed and pushed them or have some other backup.

### Stop / Uninstall
- Shut down the VM graphically or with `vagrant halt`.
- To delete the VM (including all data !!!): `vagrant destroy`. **Caution**: Any changes to the Inkscape sourcecode that you did inside the VM will be lost.
- To also free the disk space taken by the the VM base image: `vagrant box remove gusztavvargadr/windows-11`

# Advanced Information

## Customization
- Optionally, edit the `Vagrantfile` to adjust VM settings like RAM, CPU, clipboard, drag-and-drop and shared folders.
  - For Hyper-V: when using shared folders, Vagrant will ask you for the username and password of your host PC to set up the shares.
- For non-english keyboard layout, search for "keyboard" in the Vagrantfile and adjust accordingly.

## Troubleshooting

- To retry the setup and compilation: See "Reset" above.
- Warning message: *Exception calling "Read" with "3" argument(s): "Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection."*
  - This message can be ignored. It is probably from Vagrant and not from the script itself.
