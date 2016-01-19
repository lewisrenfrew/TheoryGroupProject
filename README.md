# TheoryGroupProject

Currently this is just a skeleton implementation.


# Downloading and working on the code

By now you all should have your forks of the repo, these are the place where you commit your code changes.

The following assumes your username is User on github

Clone your fork to your harddrive:

```bash
git clone https://github.com/User/TheoryGroupProject.git
```

You're going to want to keep your fork up to date with the central (my) repo:

```bash
# This only needs doing once
git remote add upstream https://github.com/Goobley/TheoryGroupProject.git
```

Then to update:

```bash
# Make sure you're on your master branch
git checkout master
# Merge
git merge upstream/master
```

When you've made some changes you need to commit:

```bash
# Add the changed files, this command adds all changed files, but no new files
git add --update
# Otherwise, add the files by name
# Commit the changes
git commit -m "Your commit message here"
# Send it back to the server, you'll need to log in to do this
git push origin master
```

Now send me a pull request with your changes :)

**If something in your repo goes wrong:**

If you have no idea how to fix it and just want to revert to the previous commit:

**Dangerous**

```bash
git reset --hard
```

If you want to keep changes:

```bash
git stash
git reset --hard
git stash pop
```
# Code standards

- We use a BSD/Allman code indentation style with 4--space indentation throughout, for reasons of
uniformity it is recommended to stick to this style when modifying the source code.

- The (\* and \&) symbols defining an item as a pointer or a reference should be attached to the
type of the variable and not the variable name i.e. `const char* str` and `const std::string& str`.

- References in function calls must be `const`! It can be infuriating to pass an object to a function
and have it be unexpectedly modified by reference. If an object is to be passed to a function to
be modified then it is to be passed as a pointer -- this makes it clear what is going on. If an
object is being passed by reference to save on construction/destruction then just make the
function take a `const` reference.

- `///` in a header is used to mark a short amount of documentation about the following item for
  Doxygen documentation.

- Type and class names are written in `PascalCase`. Variable names are in `camelCase`, variables
  that are private class members are in `camelCaseWithTrailingUnderscore_`. Function names are
  `PascalCase` throughout. Enumerations are in `SHOUTY_CASE`.

- Header files that are only compatible with C++ have the extension `hpp`, if a file is to be used
in conjunction with C code then it should be labelled `h` to differentiate it.

- Integral/Fundamental types are typedef'd in the `Types.h` file. The correspondence
is as follows:

| Typename          | Size & Type             |
|-------------------|-------------------------|
| `i8`              | 8-bit signed integer    |
| `i16`             | 16-bit signed integer   |
| `i32` *or* `int`  | 32-bit signed integer   |
| `i64`             | 64-bit signed integer   |
| `u8`              | 8-bit unsigned integer  |
| `u16`             | 16-bit unsigned integer |
| `u32` *or* `uint` | 32-bit unsigned integer |
| `u64`             | 64-bit unsigned integer |
| `f32`             | 32-bit floating point   |
| `f64`             | 64-bit floating point   |

**More pragmatically:**

- If you don't know a sensible way to start a problem, then ask.

- If you don't know what something means, then ask.

- All semi-major functions need to start with a comment block -- it justifies their existence (if
  they're too short to merit it, then they probably should be a lambda or an inline class member).

- Don't use a class where a function will suffice.

- Create compound data types to combine state for clarity and shorter function signatures.

- Flat is better than nested.

- Explicit is often better than implicit, unless it's just noise.
