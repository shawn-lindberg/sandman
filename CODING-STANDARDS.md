# Coding Standards

What follows is intended to document as clearly as possible the coding standards used by the Sandman project. As Sandman is comprised of multiple programming languages, there will be a section dedicated to each. However, there may be cases where a particular point may not be clear. In that case, please consult with the project leader.

## C/C++

For C and C++ code there is a [`.clang-format` file](sandman/.clang-format) which does match the coding standard to the extent that clang-format supports. However, there are aspects of the coding standard that clang-format does not cover, for example naming conventions.

### Column Width

The maximum column width is 100 characters.

### Tabs Versus Spaces

Always use tabs instead of spaces. The tab width is three characters.

### Naming

Class, function, and lambda names should follow this style:

```cpp
class EveryWordCapitalized
{
};

struct AcronymsAllCapitalizedJSON
{
};

bool MyJSONFunction();

auto MyLambda = [](float argument)
{

};
```

Constants and enumeration values should follow this style:

```cpp
#define kFollowedByEveryWordCapitalized 1

constexpr int kMyIntegerConstant = 0;

enum MyEnum
{
   kMyValue
};
```

However, if a define is used not as a constant value but, for example, for conditional compilation it should use the following style:

```cpp
#define ENABLE_DEBUG_CODE
```

Template parameters use the same style as classes for types, and the same style as constants for values as illustrated below. In most cases type parameters should have the suffix Type.

```cpp
template <typename ObjectType, int kMaxSize>
class MyContainer
{
};
```

Variables generally use camel case, but sometimes have a prefix:

```cpp
// Global.
float g_myGlobalVariable = 0.0f;

// Static.
static bool s_myStaticVariable = false;

// Member variable in a structure or class.
unsigned int m_myMemberVariable = 1;

// Static member variables.
unsigned int ms_widgetCount = 0;

// Function parameters.
void MyFunction(float functionParameter);

// Local variables inside a function/lambda.
bool shouldContinue = false;
```

### Formatting

Braces should always go on their own line.

```cpp
if (done == true)
{
   // Do stuff.
}
```
No omission of braces.

```cpp
if (done == true)
   s_exit = true; // Wrong.
```

Regarding spaces, binary and tertiary operators should always have spaces between them like this:

```cpp
int score = multiplier * (points + bonus);
```

Unary operators should not have spaces.

```cpp
int negative = -positive;
```

Pointer and reference specifiers go with the type, not the variable. They also shouldn't float:

```cpp
MyClass* myPointer = nullptr; // Right.
MyClass *myPointer = nullptr; // Wrong.
MyClass * myPointer = nullptr; // Wrong.
```

`const` should always be to the right of the thing that is constant.

```cpp
// Both the characters and the pointer are constant.
char const* const string = "Yep";

bool const doCoolThings = true;
```

Inside of a switch, cases should use braces if they contain more than a control flow statement. Indicate fall through cases as below.

```cpp
switch (myValue)
{
   case 0: [[fallthrough]];
   case 1:
      {
         MyFunction1();
         myCounter++;
      }
      break;

   case 2:
      {
         MyFunction2();
      }
      break;

   default:
      return;
}
```

### Wrapping

Because of the maximum column width, there will be times when a line is too long to fit within that space. The following is intended to give some guidance as to how to wrap lines.

Wrap after operators, not before.

```cpp
// Right.
float const average = total /
                      count;
float const average = 
   total / count;

// Wrong.
float const average = total
                      / count;
float const average
   = total / count;
```

Try to fit as many function arguments on each line as possible.

```cpp
// Right.
void MyExtremelySuperDuperLongFunctionNameWithArguments(float argument1, float argument2,
                                                        float argument3);

// Wrong.
void MyExtremelySuperDuperLongFunctionNameWithArguments(float argument1,
                                                        float argument2,
                                                        float argument3);
```

Also please notice that the wrapped lines are aligned.

Don't break a line with parentheses.

```cpp
// Wrong.
MyFunction(
   argument1, argument2
);

// Wrong.
MyFunction
   (
      argument1, argument2
   );
```

String literals that are too long to fit on a line should be separated like follows.

```cpp
static char const* s_string = "This is a very long and descriptive string which has no other "
                              "purpose than to demonstrate how to wrap a string literal.";
```

There are cases where following these wrapping rules will exceed the column width. Wrapping rules take priority.

### Other

Only declare one variable or constant per line.

```cpp
// Right.
int myVariable1;
int myVariable2;

// Wrong.
int myVariable1, myVariable2;
```