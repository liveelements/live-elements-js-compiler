# Contributing

## Subtrees

This repo can be used to update `lvbase` and `lvelements-compiler` repositories, as they are included as subtrees.

It's easier to have an alias for both subtrees:

```sh
git remote add origin-base https://github.com/live-keys/lvbase.git
git remote add origin-compiler https://github.com/live-keys/lvelements-compiler.git
```

The alias can be used to push to one of the subtrees:

```sh
git subtree push --prefix=lib/lvbase origin-base master
git subtree push --prefix=lib/lvelements/compiler origin-compiler master
```

Or to pull from one of the subtrees:

```sh
git subtree pull --prefix=lib/lvbase origin-base master
git subtree pull --prefix=lib/lvelements/compiler origin-compiler master
```

Note: Changes outside any subtree will not produce a commit to that particular subtree:

```sh
git add README.md
git commit -m "Added readme."
git subtree push --prefix=lib/lvbase origin-base master # will not produce a commit
```

