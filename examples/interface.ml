interface Animal:
    fn makeSound() -> string

class Dog implements Animal:
    public init():
        print("A dog has been created.")

    public fn makeSound() -> string:
        return "Woof!"

fn main():
    var dog = new Dog()
    print(dog.makeSound())
