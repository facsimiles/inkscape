[Inkscape Developer Documentation](doc/readme.md) /

Contributing to Inkscape
========================

Inkscape welcomes your contributions to make it an even better
drawing program for the Open Source community.

You can help improve Inkscape with and without programming knowledge. We need both to get a good result.

As a **non-programmer**, you can e.g. help with support, testing, documentation, translations or outreach.
Please see our [Contributing page](https://inkscape.org/contribute/).

If you want to help as a **programmer**, then please follow the rest of this page.

Contact
-------

Feel free to reach out to us.

- Chat: The development team channel is available via [Web Chat](https://chat.inkscape.org/channel/team_devel) or via IRC: irc://irc.libera.chat/#inkscape-devel
- Mailing list: [inkscape-devel@lists.inkscape.org](mailto:inkscape-devel@lists.inkscape.org). Most of the developers are subscribed to the mailing list.
- Bug tracker:
  - Report issues in the [inbox](https://gitlab.com/inkscape/inbox/-/issues).
  - Once issues are confirmed, the developers move them to the [development issue tracker](https://gitlab.com/inkscape/inkscape/-/issues)
- Video conference: We regularly meet by video. Please ask in the chat for details.
- Real life: About once a year there is an Inkscape summit. We also take part in events like Libre Graphics Meeting. This is announced in the chat and mailing list.

Git Access
----------

Inkscape is currently developed on Git, with the code hosted on GitLab.

 * https://gitlab.com/inkscape/inkscape

We give write access out to people with proven interest in helping develop
the codebase. Proving your interest is straightforward:  Make two
contributions and request access.

Compiling and Installing
------------------------

See [INSTALL.md](INSTALL.md).

Patch Decisions
---------------

Our motto for changes to the codebase is "Patch first, ask questions
later". When someone has an idea, rather than endlessly debating it, we
encourage folks to go ahead and code something up (even prototypish).
You would make this change in your own fork of Inkscape (see GitLab docs about
how to fork the repository), in a development branch of the code, which can be
submitted to GitLab as a merge request. Once in an MR it is convenient for other
folks to try out, poke and prod, and tinker with. We figure, the best
way to see if an idea fits is to try it on for size.
So if you have an idea, go ahead and fork Inkscape and submit a merge request!
That will also take care of running build tests on the branch, although we would
encourage you to check if everything builds and tests pass on your system first.

Coding Style
------------

Please refer to the [Coding Style Guidelines](https://inkscape.org/en/develop/coding-style/) if you have specific questions
on the style to use for code. If reading style guidelines doesn't interest
you, just follow the general style of the surrounding code, so that it is at
least consistent.

Documentation
-------------

Code needs to be documented. Future Inkscape developers will really
appreciate this. New files should have one or two lines describing the
purpose of the code inside the file.

Profiling
---------

[Profiling](https://en.wikipedia.org/wiki/Profiling_(computer_programming)) is a technique to find out which parts of the Inkscape sourcecode take very long to run.

1. Add `-DWITH_PROFILING=ON` to the CMake command (see [Compiling Inkscape](doc/building/readme.md)).
2. Compile Inkscape (again).
3. Run Inkscape and use the parts that you are interested in.
4. Quit Inkscape. Now a `gmon.out` file is created that contains the profiling measurement.
5. Process the file with `gprof` to create a human readable summary:
```
gprof install_dir/bin/inkscape gmon.out > report.txt
```
6. Open the `report.txt` file with a text editor.

Testing
-------

Before landing a patch, the unit tests should pass. You can run them with

```bash
ninja check
```

GitLab will also check this automatically when you submit a merge request.

If tests fail and you have not changed the relevant parts, please ask in the [chat](#contact).

Extensions
----------

All Inkscape extensions have been moved into their own repository. They
can be installed from there and should be packaged into builds directly.
Report all bugs and ideas to that sub project.

[Inkscape Extensions](https://gitlab.com/inkscape/extensions/)

They are available as a sub-module which can be updated independently:

```sh
git submodule update --remote
```

This will update the module to the latest version and you will see the
extensions directory is now changes in the git status. So be mindful of that.


Submodules / Errors with missing folders
----------------------------------------
Make sure you got the submodules code when fetching the code 
(either by using `--recurse-submodules` on the git clone command or by running `git submodule init && git submodule update --recursive`)


General Guidelines for developers
----------------------------------

* If you are new, fork the inkscape project (https://gitlab.com/inkscape/inkscape) and create a new branch for each bug/feature you want to work on. Try to Set the CI time to a high value like 2 hour (Go to your fork > Settings > CI/CD > General Pipelines > Timeout)
* Merge requests (MR) are mandatory for all contributions, even the smallest ones. This helps other developers review the code you've written and check for the mistakes that may have slipped by you.
* Before working on anything big, be sure to discuss your idea with us ([IRC](irc://irc.freenode.org/#inkscape) or [RocketChat](https://chat.inkscape.org/)). Someone else might already have plans you can build upon and we will try to guide you !
* Adopt the coding style (indentation, bracket placement, reference/pointer placement, variable naming etc. - developer's common sense required!) of existing source so that your changes and code doesn't stand out/feel foreign.
* Carefully explain your ideas and the changes you've made along with their importance in the MR. Feel free to use pictures !
* Check the "Allow commits from members who can merge to this target branch" option while submitting the MR.
* Write informative commit messages ([check this](https://chris.beams.io/posts/git-commit/)). Use full url of bug instead of mentioning just the number in messages and discussions.
* Try to keep your MR current instead of creating a new one. Rebase your MR sometimes.
* Inkscape has contributors/developers from across the globe. Some may be unavailable at times but be patient and we will try our best to help you. We are glad to have you!
