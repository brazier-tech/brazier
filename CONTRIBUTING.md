# Contributing to brazier

 Thank you for considering contributing to brazier! It's people
 like you that make brazier a great tool.

## General Guideline

### Reporting Issues

If you have found what you think is a bug, please [start a bug report discussion](https://github.com/brazier-tech/brazier/discussions/new?category=bug-reports).

For any usage questions, [ask a question in a discussion](https://github.com/brazier-tech/brazier/discussions/new?category=q-a).

### Suggesting new features

If you are here to suggest a new feature, first [suggest it in a discussion](https://github.com/brazier-tech/brazier/discussions/new?category=ideas). Please avoid submitting a duplicate request by checking if someone has already suggested a similar feature. From there, we will discuss the feature's use cases and then decide how it could be implemented.

### Committing

We apply the [conventional commit spec](https://www.conventionalcommits.org/en/v1.0.0/) across all our repositories. In brief, each commit must use one of the following types:

- **feat**: add a new feature
- **fix**: fix a bug
- **refactor**: change the code without fixing a bug or adding a feature
- **chore**: change the build process, configuration, dependencies, CI/CD pipelines, or supporting tools
- **docs**: make a documentation-only change
- **test**: add missing tests or correct existing tests

If you are unfamiliar with the usage of conventional commits, the short version is to simply specify the type as a first word, and follow it with a colon and a space, then start your message with a lowercase letter, like this:
```
feat: add support for 'foo'
```
You can also specify the scope of the commit in the parentheses after a type:
```
fix(tls): prevent use-after-free during async session teardown
```

## Development

If you would like to contribute by completing an open issue or developing a new feature, you can use this suggested workflow:

1. Fork this repository and create a new branch based off the main branch.
2. Make your own `vcpkg-configuration.json` (You can use `vcpkg-configuration.example.json``).
3.  Run `vcpkg install` to install all necessary dependencies.
4. Create failing tests for your fix or new feature in a separate folder in the [tests](https://github.com/brazier-tech/brazier/tree/master/brazier/tests/unit) folder.
5. Implement your changes.
6. Run Cmake build and wait until the project is successfully built.
7. Run the tests by running `your_build_folder/brazier_tests` and ensure they pass.
8. Git stage your changes and commit (review the commit guidelines).
9. Open a PR from your fork branch for review and wait for successful checks. 

Please document all public functions, classes, methods, templates and fields you add or edit with [Doxygen](https://www.doxygen.nl/)-style comments. They should explain what your entity does, what parameters it accepts, and what it returns/throws.
```cpp
/**
 * @brief Computes the arithmetic mean of an array of numeric values.
 *
 * Sums all elements in the provided vector using floating-point arithmetic
 * and divides by the total number of elements.
 *
 * @param[in] values A constant reference to a vector of double-precision values.
 * @return The arithmetic mean as a @c double.
 * @throws std::invalid_argument If the vector is empty.
 */
double meanAvg(const std::vector<double>& values);
```

At the beginning of each file, please include the following licence text:
```cpp
/**
 * Copyright (c) 2026 Kirill Sergeev, Nikolay Sugonyako, Andrey Agarkov, Hlib Safiannikov.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This source code is licensed under the LGPL license found in the
 * LICENSE file in the root directory of this source tree.
 */
```

## Pull requests

Please open a pull request from your fork branch when you have a change ready. For bugs, questions, or feature suggestions, you can use [discussions](https://github.com/brazier-tech/brazier/discussions). If you are unsure whether a change is appropriate, feel free to start a discussion first, or comment on the relevant issue.

Please try to keep your pull request focused in scope and avoid including unrelated commits. Pull requests will lacking scope or including many unrelated commits will be closed.

We will review the pull request and may suggest changes or request improvements. When you open a pull request, please make sure to check ✅ ["Allow edits from maintainers"](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork) on it.


Thank you for contributing to brazier!
