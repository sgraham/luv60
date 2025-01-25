luv60 compiler
==============

Experimental language and compiler.

"luv" is the name of the language, "60" is a reference to the original
motivation for this project which was ["why isn't compiling
60fps"](https://mstdn.social/@sgraham/113676168780709161).

Build and test
--------------

On Windows x64 to build and run tests (requires `python` in PATH):
```
> m r test
```

On macOS aarch64 (requires `python3` in PATH):
```
$ ./m r test
```

Ordered goals
-------------

1. Fast compilation: Stay in the >= 1M LoC/s range. Currently on a M3 MacBook
   Air, a silly benchmark is ~2.2M LoC/s to -O1 and >= 10M LoC for
   `--syntax-only` (TODO: link, see `hyperfine 'out/mr/luvc
   out/mr/dumbbench.luv'`)
2. Fast iteration: Integration with a determinism-based ("saved inputs") engine,
   incremental code update, debugger integration.
3. Low-level feel: Not trying to be much fancier than C. Try to help avoid
   common problems, but you can definitely still pointer-bash and corrupt memory
   when you want to. No intricate object model or GC, it's all just bytes.
4. Feels like Python, ergonomics-wise. Built-in list, dict, str, with arena
   allocation built-ins.

Editor support
--------------

Add:
```vimrc
set runtimepath^=.../luv60/misc/vim
```
to your `.vimrc` to get indenting and syntax highlighting.

What will it look like
----------------------

***Note: copied from old compiler prototype implementation, not much of this is
implemented in this version yet!***

(syntax highlighting is tagged as "python" on GitHub, so not 100% accurate):

### Example 1: String wrangling

- Gather some elements in a dictionary (aka map, unordered\_set, dict) mapping strings to
  strings
- Iterate over all of them, building up a slice (aka list, vector) of strings
  using Swift-style string interpolation.
- Sort the slice, and then print it out.

```python
def int main():
    {str}str mymap = {"zip": "zap", "flim": "flam", "oink": "zoink"}
    []str res
    for a, b in mymap:
        res.append("key: \(a) -> value: \(b)")
    res.sort()
    print "\n".join(res)
```
Output:
```
key: flim -> value: flam
key: oink -> value: zoink
key: zip -> value: zap
```

### Example 2: List comprehensions

- Some `auto`-like type deduction
- Terse list comprehensions
- Nested functions with access to parent variables (no heap allocations though,
  you can't return `helper`)

```python
def int main():
    cutoff = 20

    def bool helper(int x):
        return x < cutoff

    all = [i for i in range(100)]
    filt = [i for i in all if i % 2 == 1 and helper(i)]
    print filt
```
Output:
```
[1, 3, 5, 7, 9, 11, 13, 15, 17, 19]
```

### Example 3: structs and universal call syntax

- structs are just POD
- but, pseudo-methods can be added using `on` to dispatch on the static type
- various ways of initializing structs

```python
struct MyStuff:
    int a
    bool b

on MyStuff def int get_full_value(self):
    ret = self.a
    if self.b:
        ret *= 16
    return ret

def int main():
    MyStuff mystuff1
    mystuff2 = MyStuff()
    mystuff3 = MyStuff(0, false)
    mystuff4 = MyStuff(a=10)
    mystuff5 = MyStuff(a=10, b=true)

    print mystuff1.get_full_value()
    print mystuff2.get_full_value()
    print mystuff3.get_full_value()
    print mystuff4.get_full_value()
    print mystuff5.get_full_value()
```
Output:
```
0
0
0
10
160
```
