# C++ Coding Style Guidelines
* unix-style line breaks, UTF-8 file encoding
* indent with two spaces (no tabs)
* `{` should be at the end of the relevant line, not on a new line
* opening parenthesis after a control statement should not be preceded by a space -
  `if(` (good), `for(` (good), `while(` (good), `if (` (bad), etc.
* always indent the contents of a `switch` statement, and likewise always indent the contents of
  each `case` unless it is a one-liner (which is totally fine);
* prefer breaking/returning early over complicated if/else chains
* prefer `for(;;) {` for infinite loops
* indent one space after `//` when commenting (`// example comment`)
* prefer `//` over multi-line comment syntax (looks better)
* prefer no blank lines within functions, but acceptable if things get crazy
* prefer one-line if/while/etc statements when possible
* prefer two-line if/else when possible
* separate functions and classes with a single blank line
* don't indent blank lines

## Example:

```C++
if(boolean_expression) i++;
else cool_thing();
if(!another_expression) {
  crazy_variable--;
  awesome_stuff();
} else if(other_thing) {
  wicked();
} else {
  thats_cool();
}
switch(state) {
  case 0:
    std::cout << "it was zero" << std::endl;
    break;
  case 1:
    std::cout << "it was one" << std::endl;
    break;
}
while(something) something_else();
for(;;) {
  // infinite loop
  if(condition) return;
}
```
