fn main():
    print("=== Aufruf: hey() ===")
    hey()

    print("=== Aufruf: hey(123) ===")
    hey(123)

    print("=== Aufruf: hey(Pia) ===")
    hey("Pia")

fn hey() -> void:
    print("Hello!")

fn hey(x: int) -> void:
    print("int hey")
    print(x)

fn hey(x: string) -> void:
    print("string hey")
    print(x)
