function fib(n) {
    if (n < 2) return n;
    return fib(n - 2) + fib(n - 1);
}

print(fib(10));

function makeClosure() {
    var a = 0;
    function inner() {
        a = a + 1;
        return a;
    }
    return inner;
}

var inner = makeClosure();
print(inner());
print(inner());
print(inner());
