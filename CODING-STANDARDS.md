# Coding Standards

What follows is intended to document as clearly as possible the coding standards used by the Sandman project. As Sandman is comprised of multiple programming languages, there will be a section dedicated to each. However, there may be cases where a particular point may not be clear. In that case, please consult with the project leader.

## C/C++

For C and C++ code there is a [`.clang-format` file](sandman/.clang-format) which does match the coding standard to the extent that clang-format supports. However, there are aspects of the coding standard that clang-format does not cover, for example naming conventions.

### Column Width

The maximum column width is 100 characters.

### Tabs Versus Spaces

Always use tabs instead of spaces. The tab width is three characters.

### Naming

Class and function names should follow this style:

```cpp
class EveryWordCapitalized
{
};

struct AcronymsAllCapitalizedJSON
{
};

bool MyJSONFunction();
```

Constants and enumeration values should follow this style:

```cpp
#define EVERY_WORD_CAPITALIZED_WITH_UNDERSCORES_BETWEEN

constexpr int MY_INTEGER_CONSTANT = 0;

enum MyEnum
{
	MY_VALUE
};
```

Variables generally follow the same style as classes and functions, but always have a prefix:

```cpp
// Global.
float g_MyGlobalVariable = 0.0f;

// Static.
static bool s_MyStaticVariable = false;

// Member variable in a structure or class.
unsigned int m_MyMemberVariable = 1;

// Static member variables.
unsigned int ms_WidgetCount = 0;

// Function parameters.
void MyFunction(float p_FunctionParameter);

// Local variables inside a function/lambda.
bool l_ShouldContinue = false;
```

### Formatting

Braces should always go on their own line.

```cpp
if (l_Done == true)
{
	// Do stuff.
}
```
No omission of braces.

```cpp
if (l_Done == true)
	s_Exit = true; // Wrong.
```

Regarding spaces, binary and tertiary operators should always have spaces between them like this:

```cpp
int l_Score = l_Multiplier * (l_Points + l_Bonus);
```

Unary operators should not have spaces.

```cpp
int l_Negative = -l_Positive;
```

Pointer and reference specifiers go with the type, not the variable. They also shouldn't float:

```cpp
MyClass* l_MyPointer = nullptr; // Right.
MyClass *l_MyPointer = nullptr; // Wrong.
MyClass * l_MyPointer = nullptr; // Wrong.
```

`const` should always be to the right of the thing that is constant.

```cpp
// Both the characters and the pointer are constant.
char const* const l_String = "Yep";

bool const l_DoCoolThings = true;
```

### Wrapping

Because of the maximum column width, there will be times when a line is too long to fit within that space. The following is intended to give some guidance as to how to wrap lines.

Wrap after operators, not before.

```cpp
// Right.
float const l_Average = l_Total /
								l_Count;

// Wrong.
float const l_Average = l_Total
								/ l_Count;
```

Try to fit as many function arguments on each line as possible.

```cpp
// Right.
void MyExtremelySuperDuperLongFunctionNameWithArguments(float p_Argument1, float p_Argument2,
																		  float p_Argument3);

// Wrong.
void MyExtremelySuperDuperLongFunctionNameWithArguments(float p_Argument1,
																		  float p_Argument2,
																		  float p_Argument3);
```

Also please notice that the wrapped lines are aligned.

String literals that are too long to fit on a line should be separated like follows.

```cpp
static char const* s_String = "This is a very long and descriptive string which has no other "
										"purpose than to demonstrate how to wrap a string literal.";
```