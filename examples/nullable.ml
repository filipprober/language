fn main():
    var maybeNumber: int? = 42
    var maybeNull: int? = null
    var maybeName: string? = "Filip"

    print(maybeNumber)
    print(maybeNull)
    print(maybeName)

    var result = maybeDouble(maybeNumber)
    print(result)

    var nullResult = maybeDouble(null)

    # Prints 1
    if nullResult == null:
        print(1)
    else:
        print(0)

fn maybeDouble(x: int?) -> int?:
    if x == null:
        return null
    return x
