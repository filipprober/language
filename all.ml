# ==============================================
# MyLang - Complete Feature Demo
# ==============================================

# ----------------------------------------------
# 1. Basic Arithmetic Operations
# ----------------------------------------------
fn testArithmetic() -> int:
    var a = 10
    var b = 5
    var sum = a + b
    var diff = a - b
    var prod = a * b
    var quot = a / b
    var mod = a % b

    print("=== Arithmetic Tests ===")
    print(sum)
    print(diff)
    print(prod)
    print(quot)
    print(mod)

    return sum

# ----------------------------------------------
# 2. Comparison Operations
# ----------------------------------------------
fn testComparisons(x: int, y: int):
    print("=== Comparison Tests ===")
    print(x == y)
    print(x != y)
    print(x < y)
    print(x <= y)
    print(x > y)
    print(x >= y)

# ----------------------------------------------
# 3. Boolean Operations
# ----------------------------------------------
fn isEven(n: int) -> bool:
    var remainder = n % 2
    if remainder == 0:
        return true
    return false

fn isPositive(n: int) -> bool:
    if n > 0:
        return true
    return false

# ----------------------------------------------
# 4. If-Else Statements
# ----------------------------------------------
fn checkNumber(n: int):
    print("=== If-Else Test ===")
    if n > 0:
        print("Positive")
    else:
        print("Not positive")

fn categorizeNumber(n: int) -> string:
    if n > 0:
        return "positive"
    else:
        if n < 0:
            return "negative"
        else:
            return "zero"

# ----------------------------------------------
# 5. While Loops
# ----------------------------------------------
fn countDown(n: int):
    print("=== While Loop Test ===")
    while n > 0:
        print(n)
        n = n - 1

fn sumUpTo(n: int) -> int:
    var sum = 0
    var i = 1
    while i <= n:
        sum = sum + i
        i = i + 1
    return sum

# ----------------------------------------------
# 6. String Operations
# ----------------------------------------------
fn greetPerson(name: string, age: int):
    print("=== String Test ===")
    print("Hello")
    print(name)
    print("You are")
    print(age)
    print("years old")

# ----------------------------------------------
# 7. Multiple Parameters
# ----------------------------------------------
fn add(a: int, b: int) -> int:
    return a + b

fn multiply(a: int, b: int) -> int:
    return a * b

fn calculate(a: int, b: int, c: int) -> int:
    var temp = add(a, b)
    return multiply(temp, c)

# ----------------------------------------------
# 8. Nested Function Calls
# ----------------------------------------------
fn max(a: int, b: int) -> int:
    if a > b:
        return a
    return b

fn min(a: int, b: int) -> int:
    if a < b:
        return a
    return b

fn clamp(value: int, minVal: int, maxVal: int) -> int:
    var temp = max(value, minVal)
    return min(temp, maxVal)

# ----------------------------------------------
# 9. Factorial (Recursive-style with loops)
# ----------------------------------------------
fn factorial(n: int) -> int:
    var result = 1
    var i = 1
    while i <= n:
        result = result * i
        i = i + 1
    return result

# ----------------------------------------------
# 10. Complex If-Else Chains
# ----------------------------------------------
fn getGrade(score: int) -> string:
    if score >= 90:
        return "A"
    else:
        if score >= 80:
            return "B"
        else:
            if score >= 70:
                return "C"
            else:
                if score >= 60:
                    return "D"
                else:
                    return "F"

# ----------------------------------------------
# 11. Void Functions
# ----------------------------------------------
fn printBanner():
    print("======================")
    print("  MyLang Compiler")
    print("======================")

fn printSeparator():
    print("----------------------")

# ----------------------------------------------
# 12. Variable Declarations with Type Inference
# ----------------------------------------------
fn testVariables():
    var x = 42
    var y = 10
    var z = x + y
    print("=== Variable Test ===")
    print(z)

# ----------------------------------------------
# 13. Main Function - Entry Point
# ----------------------------------------------
fn main() -> int:
    printBanner()

    # Test arithmetic
    var result = testArithmetic()
    printSeparator()

    # Test comparisons
    testComparisons(10, 20)
    printSeparator()

    # Test booleans
    print("=== Boolean Tests ===")
    print(isEven(4))
    print(isEven(7))
    print(isPositive(5))
    print(isPositive(-3))
    printSeparator()

    # Test if-else
    checkNumber(5)
    checkNumber(-3)
    printSeparator()

    # Test categorize
    print("=== Categorize Test ===")
    print(categorizeNumber(10))
    print(categorizeNumber(-5))
    print(categorizeNumber(0))
    printSeparator()

    # Test while loop
    countDown(5)
    printSeparator()

    # Test sum
    print("=== Sum Test ===")
    var sum = sumUpTo(10)
    print(sum)
    printSeparator()

    # Test strings
    greetPerson("Filip", 25)
    printSeparator()

    # Test multiple params
    print("=== Calculate Test ===")
    var calc = calculate(2, 3, 4)
    print(calc)
    printSeparator()

    # Test min/max/clamp
    print("=== Min/Max/Clamp Test ===")
    print(max(10, 20))
    print(min(10, 20))
    print(clamp(15, 10, 20))
    print(clamp(5, 10, 20))
    print(clamp(25, 10, 20))
    printSeparator()

    # Test factorial
    print("=== Factorial Test ===")
    print(factorial(5))
    printSeparator()

    # Test grading
    print("=== Grade Test ===")
    print(getGrade(95))
    print(getGrade(85))
    print(getGrade(75))
    print(getGrade(65))
    print(getGrade(55))
    printSeparator()

    # Test variables
    testVariables()

    print("=== All Tests Complete ===")
    return 0