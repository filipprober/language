# Programmiersprache Spezifikation v0.1.0

## Überblick

Eine moderne, typsichere Programmiersprache mit Python/Mojo-inspirierter Syntax, starkem OOP-Support und asynchroner Programmierung.

**Kernfeatures:**
- Python/Mojo-style Indentation (4 spaces)
- Optionale Typisierung mit Type Inference
- Classes, Traits, Interfaces, Abstract Classes
- Pattern Matching mit Guards
- Async/Await
- String Interpolation
- Exception Handling
- Enums mit Auto-Increment
- Builder Pattern Support
- Standard Library auto-importiert

---

## 1. Grundlegende Syntax

### Namespaces und Imports

```python
namespace Models
namespace Core.Collections
namespace Http.Server
```

#### Imports
```python
# Core.* ist automatisch verfügbar (kein Import nötig)
# Nur für custom/externe Module:
use Database.Connection as DB
use Utils.Logger as Log
use MyLib.{Parser, Validator}
```

### Primitive Types
```
int, float, string, bool, void, any
```

### Variablen (optionale Typisierung)

```python
# Mit Type annotation
var name: string = "Max"
var age: int = 25
var price: float = 99.99
var active: bool = true

# Ohne Type annotation (Type inference)
var name = "Max"        # Compiler inferiert: string
var age = 25            # Compiler inferiert: int
var items = [1, 2, 3]   # Compiler inferiert: list[int]
var mixed = [1, "hi"]   # Compiler inferiert: list[any]
```

### Kommentare

```python
# Single line comment
// Single line comment (beide unterstützt)

/* Multi-line
   comment */

class User:
    public var name: string  # Inline comment
```

---

## 2. String Interpolation

**Syntax:** `@"...{expression}..."`

```python
var name = "Max"
var age = 25

# Basic interpolation
var greeting = @"Hello {name}!"
var info = @"Name: {name}, Age: {age}"

# Expressions
var result = @"Sum: {10 + 20}"
var status = @"Active: {age >= 18}"

# Method calls
var user = User("Alice")
var msg = @"User says: {user.greet()}"

# Multi-line
var html = @"""
<div class="user">
    <h1>{user.name}</h1>
    <p>Age: {user.age}</p>
</div>
"""

# Ohne @ = literal string (keine Interpolation)
var plain = "Hello {name}"  # Output: "Hello {name}"
```

---

## 3. Classes

### Basic Class

```python
class User:
    public var name: string
    public var email: string
    var id: int                     # private (default)

    public init(name: string, email: string):
        this.name = name
        this.email = email
        this.id = this.generateId()
    
    # Private constructor
    init(id: int, name: string):
        this.id = id
        this.name = name
    
    # Public method
    public fn greet() -> string:
        return @"Hello, {this.name}"
    
    # Private method (default)
    fn generateId() -> int:
        return 12345
```

### Visibility (Private by Default)

```python
class Plane:
    public var name: string                     # explizit public
    protected var protectedField: float         # explizit protected
    private var privateField: int               # private (default)
    var implicitPrivate: int                    # private (default)

    public init(value: int):
        this.privateField = value
    
    public fn publicMethod():
        this.privateMethod()  # Aufruf mit this.
    
    protected fn protectedMethod():
        end
    
    fn privateMethod():  # private (default)
        end
```

**Wichtig:** Methodenaufrufe innerhalb der Klasse IMMER mit `this.`

```python
class Calculator:
    public fn add(a: int, b: int) -> int:
        return a + b
    
    public fn calculate(x: int, y: int) -> int:
        var sum = this.add(x, y)  # MUSS this. verwenden
        return sum * 2
```

### Inheritance

```python
class Animal:
    public var name: string
    public var age: int

    public init(name: string, age: int):
        this.name = name
        this.age = age
    
    public fn makeSound():
        print(@"{this.name} makes a sound")
```

```python
class Dog extends Animal:
    public var breed: string

    public init(name: string, age: int, breed: string):
        super(name, age)  # Parent constructor
        this.breed = breed
    
    public fn makeSound():  # Override
        print(@"{this.name} barks!")
```

### Const Methods (Static)

```python
class User:
    public var name: string

    public init(name: string):
        this.name = name
    
    # Const method (wie static)
    public const fn createGuest() -> User:
        return User("Guest")
    
    # Const ruft andere const auf mit const::
    public const fn createAdmin() -> User:
        var guest = const::createGuest()  # Mit const::
        guest.name = "Admin"
        return guest
```

**Usage:**
```python
# Von außen: Class.method()
var guest = User.createGuest()

# Innerhalb der Klasse: const::method()
```

---

## 4. Traits (Mixins)

```python
trait Logger:
    fn log(message: string):
        print(@"[LOG] {message}")

    fn error(message: string):
        print(@"[ERROR] {message}")

trait Timestamped:
    var timestamp: int

    fn getAge() -> int:
        return now() - this.timestamp

# Class mit Traits
class User:
    use Logger
    use Timestamped

    public var name: string
    
    public init(name: string):
        this.name = name
        this.timestamp = now()
    
    public fn greet():
        this.log(@"User greeted: {this.name}")  # Aus Logger trait
        print(@"Hello {this.name}")
```

---

## 5. Interfaces

**Interface-Methoden sind immer public (kein keyword nötig)**

```python
interface Serializable:
    fn toJson() -> string
    fn fromJson(json: string) -> this

interface Comparable:
    fn compareTo(other: this) -> int

interface Drawable:
    fn draw() -> string
    fn getWidth() -> int
    fn getHeight() -> int
```

### Implementation

```python
class User implements Serializable, Comparable:
    public var name: string
    public var age: int

    public init(name: string, age: int):
        this.name = name
        this.age = age
    
    # Interface methods müssen public sein
    public fn toJson() -> string:
        return @'{{"name":"{this.name}","age":{this.age}}}'
    
    public fn fromJson(json: string) -> this:
        # Parse JSON
        return User("Parsed", 0)
    
    public fn compareTo(other: User) -> int:
        return this.age - other.age
```

---

## 6. Abstract Classes

```python
abstract class Shape:
    public var color: string

    public init(color: string):
        this.color = color
    
    # Abstract method - muss implementiert werden
    public abstract fn area() -> float
    
    # Concrete method - kann verwendet werden
    public fn describe() -> string:
        return @"A {this.color} shape with area {this.area()}"
```

```python
class Circle extends Shape:
    public var radius: float

    public init(color: string, radius: float):
        super(color)
        this.radius = radius
    
    # Muss area() implementieren
    public fn area() -> float:
        return 3.14159 * this.radius * this.radius
```

```python
class Rectangle extends Shape:
    public var width: float
    public var height: float

    public init(color: string, width: float, height: float):
        super(color)
        this.width = width
        this.height = height
    
    public fn area() -> float:
        return this.width * this.height
```

**Usage:**
```python
# var shape = Shape("red")  # Error: Cannot instantiate abstract class
var circle = Circle("blue", 5.0)
var rect = Rectangle("red", 10.0, 20.0)
```

---

## 7. Pattern Matching & Guards

### Pattern Matching auf Werte

```python
class Calculator:
    # Factorial mit pattern matching
    public fn factorial(0) -> int:
        return 1

    public fn factorial(n: int) -> int when n > 0:
        return n * this.factorial(n - 1)
    
    # HTTP Status codes
    public fn getStatusMessage(200) -> string:
        return "OK"
    
    public fn getStatusMessage(404) -> string:
        return "Not Found"
    
    public fn getStatusMessage(500) -> string:
        return "Server Error"
    
    public fn getStatusMessage(code: int) -> string:
        return @"Unknown status: {code}"
```

### Pattern Matching auf Types (Overloading)

```python
class Formatter:
    public fn format(value: int) -> string:
        return @"Number: {value}"

    public fn format(value: string) -> string:
        return @"Text: {value}"
    
    public fn format(value: bool) -> string:
        return @"Boolean: {value}"
    
    public fn format(values: list[int]) -> string:
        return @"List with {values.length()} items"
```

### Guards mit when

```python
class Greeter:
    public fn greet(user: User) -> string when user.age >= 18:
        return @"Hello adult {user.name}"

    public fn greet(user: User) -> string when user.age >= 13:
        return @"Hello teen {user.name}"
    
    public fn greet(user: User) -> string:
        return @"Hello young {user.name}"
```

```python
class Validator:
    public fn validateAge(age: int) -> string when age < 0:
        return "Invalid age"

    public fn validateAge(age: int) -> string when age < 18:
        return "Minor"
    
    public fn validateAge(age: int) -> string when age < 65:
        return "Adult"
    
    public fn validateAge(age: int) -> string:
        return "Senior"
```

**Wichtig:** Compiler matched von oben nach unten - erste passende Funktion gewinnt!

---

## 8. Match Statement

### Basic Match

```python
fn handleRequest(method: HttpMethod) -> string:
    match method:
        case HttpMethod.GET:
            return "Fetching data"
        case HttpMethod.POST:
            return "Creating resource"
        case HttpMethod.PUT:
            return "Updating resource"
        case HttpMethod.DELETE:
            return "Deleting resource"
```

### Match mit Default

```python
fn getStatusMessage(code: int) -> string:
    match code:
        case 200:
            return "OK"
        case 404:
            return "Not Found"
        case 500:
            return "Server Error"
        default:
            return @"Unknown status: {code}"
```

### Match mit Guards

```python
fn categorizeAge(age: int) -> string:
    match age:
        case x when x < 0:
            return "Invalid"
        case x when x < 18:
            return "Minor"
        case x when x < 65:
            return "Adult"
        default:
            return "Senior"
```

### Match auf Types

```python
fn processValue(value: any) -> string:
    match value:
        case v as int:
            return @"Number: {v}"
        case v as string:
            return @"Text: {v}"
        case v as User:
            return @"User: {v.name}"
        default:
            return "Unknown type"
```

### Match mit Multiple Cases

```python
fn isWeekend(day: string) -> bool:
    match day:
        case "Saturday", "Sunday":
            return true
        default:
            return false
```

---

## 9. Anonymous Functions (Lambdas)

### Basic Syntax

**Mit `:` für normale Lambdas:**
```python
fn(parameters) -> returnType:
    body
```

**Mit `=>` für direkte Returns (ohne `return` keyword):**
```python
fn(parameters) -> returnType => expression
```

### Beispiele

```python
# Mit :
var square = fn(x: int) -> int:
    return x * x

var greet = fn(name: string) -> string:
    return @"Hello {name}"

# Mit => (direkter return)
var square = fn(x: int) -> int => x * x
var greet = fn(name: string) -> string => @"Hello {name}"
var add = fn(a: int, b: int) -> int => a + b

# Void functions
var logger = fn(msg: string) -> void:
    print(@"[LOG] {msg}")

var action = fn() -> void => print("Action executed")
```

### Multi-line Lambdas

```python
var complex = fn(x: int) -> int:
    var doubled = x * 2
    var plusOne = doubled + 1
    return plusOne
```

```python
var validator = fn(x: int) -> bool:
    if x < 0:
        return false
    if x > 100:
        return false
    return true
```

### Array Operations

```python
var numbers = [1, 2, 3, 4, 5]

# Mit =>
var doubled = numbers.map(fn(x: int) -> int => x * 2)
var evens = numbers.filter(fn(x: int) -> bool => x % 2 == 0)

# Mit :
var sum = numbers.reduce(fn(acc: int, x: int) -> int:
    return acc + x
, 0)

numbers.forEach(fn(x: int) -> void:
    print(x)
)
```

### Higher-Order Functions

```python
fn applyOperation(x: int, y: int, op: fn(int, int) -> int) -> int:
    return op(x, y)

var result1 = applyOperation(5, 3, fn(a: int, b: int) -> int => a + b)  # 8
var result2 = applyOperation(5, 3, fn(a: int, b: int) -> int => a * b)  # 15
```

### Closures

```python
fn createAdder(amount: int) -> fn(int) -> int:
    return fn(x: int) -> int => x + amount

var add5 = createAdder(5)
var add10 = createAdder(10)

print(add5(3))   # 8
print(add10(3))  # 13
```

### Function Type Signatures

```python
var callback: fn(string) -> void
var operation: fn(int, int) -> int
var supplier: fn() -> string

class Button:
    var onClick: fn(string) -> void

    public init():
        this.onClick = fn(msg: string) -> void => print(msg)
    
    public fn setOnClick(callback: fn(string) -> void):
        this.onClick = callback
```

---

## 10. Builder Pattern & Method Chaining

```python
class HttpRequest:
    var method: string
    var url: string
    var headers: dict[string, string]
    var body: string

    init():
        this.method = "GET"
        this.url = ""
        this.headers = {}
        this.body = ""
    
    public const fn builder() -> HttpRequestBuilder:
        return HttpRequestBuilder()
    
    public fn execute() -> HttpResponse:
        print(@"Executing {this.method} {this.url}")
        return HttpResponse(200, "OK")
```

```python
class HttpRequestBuilder:
    var request: HttpRequest

    public init():
        this.request = HttpRequest()
    
    # Method chaining mit -> this
    public fn withMethod(method: string) -> this:
        this.request.method = method
        return this
    
    public fn withUrl(url: string) -> this:
        this.request.url = url
        return this
    
    public fn withHeader(key: string, value: string) -> this:
        this.request.headers[key] = value
        return this
    
    public fn withBody(body: string) -> this:
        this.request.body = body
        return this
    
    public fn get(url: string) -> this:
        return this.withMethod("GET").withUrl(url)
    
    public fn post(url: string) -> this:
        return this.withMethod("POST").withUrl(url)
    
    public fn build() -> HttpRequest:
        return this.request
```

**Usage:**
```python
var response = HttpRequest.builder()
    .post("https://api.example.com/users")
    .withHeader("Content-Type", "application/json")
    .withHeader("Authorization", "Bearer token123")
    .withBody('{"name":"Max"}')
    .build()
    .execute()
```

---

## 11. any Type (wie TypeScript any)

### any für flexible Variablen

```python
var value: any = 42
value = "Hello"
value = true
value = [1, 2, 3]
```

### any in Collections

```python
var mixed: list[any] = [1, "hello", true, 3.14]
var data: dict[string, any] = {
    "name": "Max",
    "age": 25,
    "active": true
}
```

### any als Parameter

```python
fn processValue(value: any) -> string:
    if value is int:
        return @"Got int: {value}"
    if value is string:
        return @"Got string: {value}"
    if value is bool:
        return @"Got bool: {value}"
    
    return "Unknown type"
```

### API Response mit any

```python
class ApiResponse:
    public var data: any
    public var statusCode: int

    public init(data: any, statusCode: int):
        this.data = data
        this.statusCode = statusCode

var response1 = ApiResponse({"user": "Max"}, 200)
var response2 = ApiResponse([1, 2, 3], 200)
var response3 = ApiResponse("Error", 500)
```

### Type checking mit is

```python
fn checkType(value: any):
    if value is int:
        print("It's an integer")
    if value is string:
        print("It's a string")
    if value is User:
        print("It's a User object")
```

### Type casting mit as

```python
fn castExample(value: any):
    if value is int:
        var num = value as float
        print(num * 2)
```

---

## 12. Enums

### Int Enum (Auto-Increment ab 0)

```python
enum Status:
    Pending     # = 0 (automatisch)
    Active      # = 1
    Inactive    # = 2
    Deleted     # = 3

# Oder explizit typisiert
enum Status -> int:
    Pending     # = 0
    Active      # = 1
    Inactive    # = 2
    Deleted     # = 3

# Usage
var status = Status.Active
print(status)  # 1
```

### Manuell gesetzte Werte

```python
enum HttpStatus -> int:
    OK = 200
    NotFound = 404
    ServerError = 500

var code = HttpStatus.OK
print(code)  # 200
```

### String Enum (explizit)

```python
enum Role -> string:
    Admin = "admin"
    User = "user"
    Guest = "guest"

var role = Role.Admin
print(role)  # "admin"
```

### Gemischte Enums (explizit ab erstem Wert)

```python
enum Priority -> int:
    Low = 1
    Medium      # = 2 (auto)
    High        # = 3 (auto)
    Critical = 99

print(Priority.Medium)   # 2
print(Priority.Critical) # 99
```

### Enum Methods

```python
enum HttpMethod -> string:
    GET = "GET"
    POST = "POST"
    PUT = "PUT"
    DELETE = "DELETE"
    
    public fn isModifying() -> bool:
        return this == HttpMethod.POST or 
               this == HttpMethod.PUT or 
               this == HttpMethod.DELETE
    
    public fn requiresBody() -> bool:
        return this == HttpMethod.POST or this == HttpMethod.PUT

# Usage
var method = HttpMethod.POST
if method.isModifying():
    print("This modifies data")
```

### Enum mit Const Methods

```python
enum Color -> string:
    Red = "red"
    Green = "green"
    Blue = "blue"
    
    public const fn all() -> list[Color]:
        return [Color.Red, Color.Green, Color.Blue]
    
    public const fn fromString(value: string) -> Color:
        match value:
            case "red":
                return Color.Red
            case "green":
                return Color.Green
            case "blue":
                return Color.Blue
            default:
                throw ValueError(@"Invalid color: {value}")

# Usage
var colors = Color.all()
var red = Color.fromString("red")
```

---

## 13. Exception Handling

### Try/Catch/Finally

```python
try:
    var user = loadUser(42)
    user.save()
catch e as FileNotFoundException:
    print(@"File not found: {e.message}")
catch e as DatabaseException:
    print(@"Database error: {e.message}")
    rollback()
catch e as Exception:
    print(@"Unknown error: {e.message}")
finally:
    cleanup()
```

### Custom Exceptions

```python
class ValidationException extends Exception:
    public var field: string
    
    public init(message: string, field: string):
        super(message)
        this.field = field

class UserNotFoundException extends Exception:
    public var userId: int
    
    public init(userId: int):
        super(@"User {userId} not found")
        this.userId = userId

class NetworkException extends Exception:
    public var statusCode: int
    
    public init(message: string, statusCode: int):
        super(message)
        this.statusCode = statusCode
```

### Throwing Exceptions

```python
fn validateAge(age: int):
    if age < 0:
        throw ValidationException("Age cannot be negative", "age")
    if age > 150:
        throw ValidationException("Age too high", "age")

fn loadUser(id: int) -> User:
    if id < 0:
        throw UserNotFoundException(id)
    
    try:
        return database.find(id)
    catch e as DatabaseException:
        throw NetworkException("Database unreachable", 500)
```

### Try ohne Catch (nur Finally)

```python
fn withCleanup():
    var file = openFile("data.txt")
    try:
        processFile(file)
    finally:
        file.close()  # Immer ausgeführt
```

### Nested Try/Catch

```python
fn complexOperation():
    try:
        var user = loadUser(42)
        try:
            user.save()
        catch e as DatabaseException:
            print("Save failed, using cache")
            cache.store(user)
    catch e as FileNotFoundException:
        print("User not found")
```

### Exception Handling mit Async

```python
async fn safeLoad(id: int) -> User:
    try:
        return await fetchUser(id)
    catch e as NetworkException:
        print(@"Network error: {e.message}")
        return User.createGuest()
```

---

## 14. Async/Await

### Async Function Declaration

```python
# Async function
async fn fetchUser(id: int) -> User:
    var response = await http.get(@"/users/{id}")
    return User.fromJson(response.body)

# Async method in class
class UserService:
    public async fn getUser(id: int) -> User:
        var data = await this.fetchFromDatabase(id)
        return User(data.name, data.email, data.age)
    
    async fn fetchFromDatabase(id: int) -> dict[string, any]:
        await sleep(100)  # Simulate delay
        return {"name": "Max", "email": "max@test.com", "age": 25}
```

### Calling Async Functions

```python
# Calling async functions
async fn main():
    var user = await UserService().getUser(42)
    print(user.name)

# Parallel execution
async fn loadMultiple():
    var user1 = fetchUser(1)    # Start async (returns Promise)
    var user2 = fetchUser(2)    # Start async
    
    # Wait for both
    var u1 = await user1
    var u2 = await user2
    
    return [u1, u2]
```

### Async Collections

```python
async fn processUsers(ids: list[int]) -> list[User]:
    var futures = ids.map(fn(id: int) => fetchUser(id))
    return await Promise.all(futures)  # Warte auf alle
```

### Promise API

```python
# Promise.all - parallel execution
var results = await Promise.all([
    fetchUser(1),
    fetchUser(2),
    fetchUser(3)
])

# Promise.race - first to finish
var fastest = await Promise.race([
    fetchFromCache(id),
    fetchFromDatabase(id)
])

# Chaining
var user = await fetchUser(42)
    .then(fn(u: User) -> User:
        u.lastLogin = now()
        return u
    )
    .catch(fn(e: Exception) -> User:
        return User.createGuest()
    )
```

---

## 15. Control Flow

### if/else

```python
if age >= 18:
    print("Adult")
else if age >= 13:
    print("Teen")
else:
    print("Child")
```

### while

```python
var i = 0
while i < 10:
    print(i)
    i += 1
```

### for

```python
for item in items:
    print(item)

for i in range(10):
    print(i)

for i in range(0, 10, 2):  # start, end, step
    print(i)
```

### return/break/continue

```python
fn findFirst(items: list[int]) -> int:
    for item in items:
        if item < 0:
            continue
        if item > 100:
            break
        return item
    
    return -1
```

---

## 16. end Keyword

Das `end` keyword ist **nur bei leeren Blöcken Pflicht** (wie `pass` in Python).

```python
# Pflicht bei leeren Blöcken
class Empty:
    end

fn doNothing():
    end

interface Marker:
    end

# Optional bei Inline-Funktionen
fn square(x: int) -> int: return x * x end  # Mit end
fn square(x: int) -> int: return x * x      # Ohne end (auch ok)

# Nicht nötig bei Multi-line
fn calculate(x: int) -> int:
    var doubled = x * 2
    return doubled
```

---

## 17. Standard Library (Core)

### Auto-Import

Alle `Core.*` Module sind automatisch verfügbar, **kein Import nötig!**

```python
# Diese sind IMMER verfügbar:
# Core.Collections
list[T], dict[K, V], set[T]

# Core.String
string, StringBuilder

# Core.Math
int, float, abs(), sqrt(), pow()

# Core.IO
print(), println(), readLine(), readFile(), writeFile()

# Core.Async
async, await, Promise, sleep()

# Core.Exceptions
Exception, RuntimeException, ValueError, TypeError
FileNotFoundException, NetworkException
```

### Core.Collections

```python
# list[T]
var numbers = [1, 2, 3, 4, 5]
numbers.add(6)
numbers.remove(0)
var doubled = numbers.map(fn(x: int) -> int => x * 2)
var evens = numbers.filter(fn(x: int) -> bool => x % 2 == 0)
var sum = numbers.reduce(fn(acc: int, x: int) -> int => acc + x, 0)
var len = numbers.length()

# dict[K, V]
var data = {"name": "Max", "age": 25}
data.set("email", "max@test.com")
var name = data.get("name")
var hasAge = data.has("age")
data.remove("age")
var keys = data.keys()
var values = data.values()

# set[T]
var tags = set["kotlin", "java", "rust"]
tags.add("python")
tags.remove("java")
var hasKotlin = tags.has("kotlin")
var combined = tags.union(otherTags)
```

### Core.String

```python
var text = "Hello World"
var len = text.length()
var upper = text.toUpperCase()
var lower = text.toLowerCase()
var parts = text.split(" ")
var trimmed = text.trim()
var replaced = text.replace("World", "Universe")
var contains = text.contains("Hello")
var starts = text.startsWith("Hello")
var ends = text.endsWith("World")
```

### Core.IO

```python
print("Hello")
println("Hello with newline")
var input = readLine()
var content = readFile("data.txt")
writeFile("output.txt", "Hello World")
```

### Core.Async

```python
async fn example():
    await sleep(1000)  # Sleep 1 second
    
    var results = await Promise.all([promise1, promise2])
    var fastest = await Promise.race([promise1, promise2])
```

### Core.Exceptions

```python
# Base exceptions
Exception
RuntimeException
ValueError
TypeError
IndexOutOfBoundsException
FileNotFoundException
NetworkException
DatabaseException

# Custom exception
class MyException extends Exception:
    public init(message: string):
        super(message)
```

---

## 18. Operators

### Arithmetic
```python
+   # Addition
-   # Subtraction
*   # Multiplication
/   # Division
%   # Modulo
```

### Assignment
```python
=    # Assignment
+=   # Add and assign
-=   # Subtract and assign
*=   # Multiply and assign
/=   # Divide and assign
```

### Comparison
```python
==   # Equal
!=   # Not equal
<    # Less than
<=   # Less than or equal
>    # Greater than
>=   # Greater than or equal
```

### Logical
```python
and  # Logical AND
or   # Logical OR
not  # Logical NOT
```

### Type Operators
```python
is   # Type check: value is int
as   # Type cast: value as int
in   # Membership: key in dict
```

---

## 19. Keywords

```python
# Module System
namespace, use

# OOP
class, abstract, interface, trait, enum
extends, implements

# Visibility
public, private, protected

# Functions & Variables
fn, var, const, init, this, super, end

# Control Flow
if, else, while, for, match, case, default
return, break, continue, when

# Async
async, await

# Exception Handling
try, catch, finally, throw

# Types
int, float, string, bool, void, any

# Literals
true, false, null

# Operators
and, or, not, is, as, in
```

---

## 20. Naming Conventions

- **Namespaces**: PascalCase (`Models`, `Core.Collections`)
- **Classes**: PascalCase (`User`, `HttpRequest`)
- **Traits**: PascalCase (`Logger`, `Timestamped`)
- **Interfaces**: PascalCase (`Serializable`, `Comparable`)
- **Enums**: PascalCase (`Status`, `HttpMethod`)
- **Variables**: camelCase (`userName`, `isActive`)
- **Functions**: camelCase (`greet`, `validateEmail`)
- **Constants**: camelCase (`maxRetries`, `defaultTimeout`)
- **Types**: lowercase (`int`, `string`, `bool`, `any`)

---

## 21. Vollständiges Beispiel

```python
namespace Services

# Nur custom modules importieren (Core ist auto-verfügbar)
use Database.Connection as DB

# Enum mit Auto-Increment
enum UserRole:
    Guest       # 0
    User        # 1
    Admin       # 2
    SuperAdmin  # 3

enum HttpStatus -> int:
    OK = 200
    NotFound = 404
    ServerError = 500

# Custom Exception
class UserNotFoundException extends Exception:
    public var userId: int
    
    public init(userId: int):
        super(@"User {userId} not found")
        this.userId = userId

# Service Class
class UserService:
    var cache: dict[int, User]
    var db: DB.Connection
    
    public init():
        this.cache = {}
        this.db = DB.connect("localhost")
    
    # Async method
    public async fn getUser(id: int) -> User:
        # Check cache
        if id in this.cache:
            return this.cache[id]
        
        try:
            var user = await this.fetchFromDatabase(id)
            this.cache[id] = user
            return user
        catch e as NetworkException:
            print(@"Network error: {e.message}")
            throw UserNotFoundException(id)
        finally:
            print("Request completed")
    
    async fn fetchFromDatabase(id: int) -> User:
        await sleep(100)
        var result = this.db.query(@"SELECT * FROM users WHERE id = {id}")
        
        if result.isEmpty():
            throw UserNotFoundException(id)
        
        return User.fromDict(result.first())
    
    # Const method
    public const fn createDefault() -> UserService:
        return UserService()
    
    public fn getUserRole(user: User) -> string:
        match user.roleId:
            case UserRole.Guest:
                return "Guest User"
            case UserRole.User:
                return "Regular User"
            case UserRole.Admin:
                return "Administrator"
            case UserRole.SuperAdmin:
                return "Super Administrator"
            default:
                return "Unknown Role"

# Main
async fn main():
    var service = UserService.createDefault()
    
    try:
        # Parallel loading
        var user1Promise = service.getUser(1)
        var user2Promise = service.getUser(2)
        
        var users = await Promise.all([user1Promise, user2Promise])
        
        # Lambda mit String interpolation
        var names = users.map(fn(u: User) -> string => 
            @"{u.name} ({service.getUserRole(u)})"
        )
        
        print(@"Loaded users: {names.join(', ')}")
        
    catch e as UserNotFoundException:
        print(@"User {e.userId} not found")
    catch e as Exception:
        print(@"Error: {e.message}")
    finally:
        print("Done")
```

---

## 22. Token Types für Lexer

```
# Keywords
NAMESPACE, USE, CLASS, ABSTRACT, INTERFACE, TRAIT, ENUM
EXTENDS, IMPLEMENTS
PUBLIC, PRIVATE, PROTECTED
FN, VAR, CONST, INIT, THIS, SUPER, END
IF, ELSE, WHILE, FOR, MATCH, CASE, DEFAULT
RETURN, BREAK, CONTINUE, WHEN
ASYNC, AWAIT
TRY, CATCH, FINALLY, THROW
INT, FLOAT, STRING, BOOL, VOID, ANY
TRUE, FALSE, NONE
AND, OR, NOT, IS, AS, IN

# Literals
IDENTIFIER
INTEGER_LITERAL
FLOAT_LITERAL
STRING_LITERAL
INTERPOLATED_STRING

# Operators
PLUS, MINUS, STAR, SLASH, PERCENT
EQUAL, EQUAL_EQUAL, BANG_EQUAL
LESS, LESS_EQUAL, GREATER, GREATER_EQUAL
PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL

# Delimiters
LEFT_PAREN, RIGHT_PAREN
LEFT_BRACE, RIGHT_BRACE
LEFT_BRACKET, RIGHT_BRACKET
COMMA, DOT, COLON, SEMICOLON
ARROW (->), FAT_ARROW (=>), DOUBLE_COLON (::)

# Special
NEWLINE, INDENT, DEDENT
EOF_TOKEN, COMMENT, UNKNOWN
```

---

## License

MIT License - Filip Prober
