def test_function(arg1, arg2):
    print(f"Python test_function called with: {arg1}, {arg2}")
    return arg1 + arg2

def py_print():
    print("Hello from Python in CG Engine!")

result = test_function(5, 10)
print(f"Python function result: {result}")