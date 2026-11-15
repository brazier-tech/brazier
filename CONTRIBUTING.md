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

...

Please document each function and class you write with [Doxygen](https://www.doxygen.nl/)-style comments like this:
```cpp
/**
 * @brief Short summary of your function.
 *
 * Longer explanation of what foo() does.
 * It can span multiple lines and include code examples.
 *
 * @param[in]  count Read-only parameter.
 * @param[out] bar   Write parameter (this will be written to by the function).
 * @return What this function returns and in what case.
 */
bool foo(int count, std::string& bar);
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

Please do not open pull requests yourself. Use [discussions](https://github.com/brazier-tech/brazier/discussions) to report bugs, ask questions or suggest features. If you have implemented a change, share a branch from your fork. A maintainer may then open a pull request from that branch.

If a maintainer opens a pull request from your fork branch, please try to keep it focused in scope and avoid including unrelated commits. Pull requests will not be opened from fork branches lacking scope or including many unrelated commits.

We will review the pull request and may suggest changes or request improvements. When we open the pull request, please make sure to check ✅ ["Allow edits from maintainers"](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork) on your pull request.


Thank you for contributing to brazier!
