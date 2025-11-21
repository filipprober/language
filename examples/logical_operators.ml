fn main():
    var x: int = 10
    var y: int = 5

    if x >= 5 and x <= 10:
        print("x is 5 or between 5 and 10")

    if x >= 5 && x <= 10:
        print("x is 5 or between 5 and 10")

    if x == 0 or y == 0:
        print("At least one is zero")
    else:
        print("Neither is zero")

    if x == 0 || y == 0:
        print("At least one is zero")
    else:
        print("Neither is zero")

    if true or print("This should not print"):
        print("Short-circuit works")
