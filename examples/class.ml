class Person:
    public init(name: string):
        print(name)

class Developer extends Person:
    public init(name: string):
        super(name . " is a developer.")

    public fn print_role():
        print("I am a developer.")

fn main():
    var person = new Person("Alice")
    var developer = new Developer("Bob")

    developer.print_role()

    print("Person instance created.")
