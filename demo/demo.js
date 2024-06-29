class Breakfast {
    constructor(meat, bread) {
        this.meat = meat;
        this.bread = bread;
        echo(now());
        print(" now init...");
    }

    // ...
}

class Brunch extends Breakfast {
    drink() {
        print("How about a Bloody Mary?");
    }
}

Brunch(1, 2).drink();
