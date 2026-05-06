# How to contribute to Quasar?

## What is Quasar
Quasar is an open-source operating system project 
targeting x86_64 architecture. Our bootloader is 
Limine because it is the most modern and lightest 
option available. Our goal is to move forward steadily avoiding 
the bloat and dead code that plagues large projects.

## Getting Started
1. Fork the repository
2. Clone your fork
3. Install dependencies (See README.md)
4. Build the project
5. Test with QEMU

## How to Contribute
- **Bug Reports** — Open a GitHub Issue
- **Feature Requests** — Start a GitHub Discussion  
- **Security Issues** — quasaros.communication@protonmail.com
- **Code** — Open a Pull Request

## Pull Requests
- Write clear commit messages using feat:, docs: and refactor: etc.
- Test your changes with QEMU before submitting
- Make sure that added features are really working correctly
- Major changes? Start a Discussion first

## Thought
While requesting a feature or working on the project, 
ask yourself these questions:

- Is this feature needed or necessary? 
  Answer must be yes.
- Am I adding this code only because I might need it 
  in the future, without actually using it now? 
  Answer must be no.
- Is the code I wrote readable, free of magic numbers, 
  and does it do its job well? 
  Answer must be yes.

And please don't forget that this project is trying to avoid getting heavier. When in doubt, leave it out.
