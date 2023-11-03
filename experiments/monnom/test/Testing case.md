# Case Study

## Pass

### 1.Normal instance expression and some definition in the instance 

```c#
class Fruit
{
    public Int Weight;
    public String s;
    public constructor(String i,Int height){
      Weight = height;
      s = i;
    }
    public instance DefaultFruit
    {
       Weight = 10;
       s = "apple";
    }
    public multi instance OtherFruit(String name , weight)
    {
       Weight = weight;
       s = name;
     }
}
class Main{
	public static fun Main() : Void
	{
	    Fruit fruit = new Fruit(" ",10);
            fruit = Fruit::DefaultFruit(); 
            fruit = Fruit::OtherFruit("Apple",10); 
	}
}
```

### 2. Default operation expression ( +, - , *, /)

```c#
class Fruit
{
    public Int Weight;
    public String s;


    public constructor(String i,Int height){
     Weight = height;
     s = i;
    }
    public instance DefaultFruit
    {
       Weight = 10 * 2 + 2 / 2;
       Weight = 10 - 2;
       s = "apple";
    }
    public multi instance OtherFruit(Int weight)
    {
         Weight = weight;
         s = "apple";
     }
}
class Main{
	public static fun Main() : Void
	{
	    Fruit fruit = new Fruit(" ",10);
        fruit = Fruit::DefaultFruit(); 
        fruit = Fruit::OtherFruit(10); 
	}
}

```

### 3. Default expression with instance assignment 

```c#
class Fruit
{
    public Fruit f;
    public String s;
    public instance DefaultFruit
    {
       f = this;
       s = "Apple";
    }
    public multi instance OtherFruit(String a)
    {
       f = Fruit::DefaultFruit;
       s = a;
    }
}
class Main{
	public static fun Main() : Void
	{
	    
      	    Fruit fruit = Fruit::DefaultFruit(); 
            Fruit fruit2 = Fruit::OtherFruit("Banana"); 
	}
}
```

### 4. Using Instance to initialize field 

and the assignment field is using 'this'

```c#
class Fruit
{
    public Int Size;
    public String FruitName;
    public Fruit f;
    public Tree t;
    public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = Fruit::DefaultFruit;
      t = new Tree(f);
    }
    public instance DefaultFruit
    {
        size = 10;
        FruitName = "Apple";
        f = this;
        t = Tree::DefaultTree;
    }
    public multi instance OtherFruit(Int size)
    {
       Size = size;
        FruitName = "Apple";
        f = this;
        t = Tree::DefaultTree;
    }
}

class Tree{
   public String TreeName;
   public Fruit fruit;
   public constructor(Fruit f){
      fruit = f;
      TreeName = f.FruitName;
   }
}
```

### 5. need initial all the field 

```c#
class Fruit
{
    public Int Size;
    public String FruitName;
    public Fruit f;
    public Tree t;
    public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = Fruit::DefaultFruit;
      t = new Tree(f);
    }
    public instance DefaultFruit
    {
       Size = 1;
       FruitName = " ";
       f = this;
       t = Tree::DefaultTree;
    }
   
}

class Tree{
   public String TreeName;
   public Fruit fruit;
   public Tree t;
   public constructor(Fruit f){
      fruit = f;
      t = Tree::DefaultTree;
      TreeName = f.FruitName;
   }
   public instance DefaultTree{
       t = this;
       fruit = Fruit::DefaultFruit;
            TreeName = "apple";       
    }
}

class Main{
	public static fun Main() : Void
	{
	Fruit fruit = new Fruit(" ",10);
        fruit = Fruit::DefaultFruit; 
	}
}
```



## Fail



### 1. instance expression with wrong arguments

```c#
class Fruit
{
    public Fruit f;
    public String FruitName;
    public constructor (String str){
	f = Fruit::DefaultFruit;
        FruitName = str;
    }
    public instance DefaultFruit
     {
       f = this;
       FruitName = "apple";
    }
   public multi instance OtherFruit(String name)
    {
       f = Fruit::DefaultFruit;
       FruitName = name;
    }
}
class Main{
	public static fun Main() : Void
	{
	 
      	    Fruit fruit = Fruit::DefaultFruit(11); 
            Fruit fruit2 = Fruit::OtherFruit("Banana"); 
	}
}
```

ERROR:
`Fruit<> does not have Instance that match the given arguments!`





### 3. Do not have the Instance 

```c#
 public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = Fruit::AppleFruit(1,2,3);
      t = new Tree(f);
    }
```

error: `Fruit<> does not have Instance that match the given arguments!
Fruit<> does not have Instance that match the given arguments!`



cannot find the instance expression 

### 4.Instance Type

```c#
 public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = Tree::DefaultTree;   
      t = new Tree(f);
    }
```



### 5. instance assignment type wrong

```c#
class Fruit
{
    public Fruit f;
    public String FruitName;
    public constructor (String str){
	f = Tree::DefaultTree;
        FruitName = str;
    }
    public instance DefaultFruit
     {
       f = this;
       FruitName = "apple";
    }
   public multi instance OtherFruit(String name)
    {
       f = Fruit::DefaultFruit;
       FruitName = name;
    }
}

class Tree{
	public String name;
	public constructor(String n){
		name = n;
	}
	public instance DefaultTree{
		name = "AppleTree";
	}
}
class Main{
	public static fun Main() : Void
	{
	 
      	    Fruit fruit = Fruit::DefaultFruit(11); 
            Fruit fruit2 = Fruit::OtherFruit("Banana"); 
	}
}
```



error: `Result of assignment expression has wrong type for variable f (Test.mn:6:1) even optimistically!`



### instance should contain all the field in the class

```c#
class Fruit
{
    public Int Size;
    public String Name;
    public constructor(String name,Int size){
      Size = size;
      Name = name;
    }

   private instance DefaultFruit{
	Size = 10;
   }
}
```

error `All fields in DefaultFruit (Test.mn:10:20) must be declared`



### Super class

finished

```C#
class Fruit
{
    public Int Size;
    public String Name;
    public constructor(String name,Int size){
      Size = size;
      Name = name;
    }

   public instance DefaultFruit{
	Size = 10;
        Name = "APple";
   }
}
class Apple extends Tree{
     public Int Size;
     public String Name;	

   public multi instance FruitApple(Int s,String x) extends DefaultAppleTree{
	Size = 10;
	Name = "";
   }
}

class Tree{
	public String name;
	public constructor(String n){
		name = n;
	}
	public instance DefaultTree{
		name = "AppleTree";
	}
}
class Main{
	public static fun Main() : Void
	{
	 
      	    Fruit fruit = Fruit::DefaultFruit(); 
         
	}
}
```

error : `Extends is invalid, can not find instance DefaultAppleTree (Test.mn:19:60) in class FruitApple (Test.mn:19:25)`



### private instance

```c#
class Fruit
{
    public Int Size;
    public String Name;
    public constructor(String name,Int size){
      Size = size;
      Name = name;
    }

   private instance DefaultFruit{
	Size = 10;
	Name = "";
   }
}
class Apple extends Fruit{
     public Int Size;
     public String Name;	

   public multi instance FruitApple(Int s,String x) extends DefaultFruit{
	Size = 10;
	Name = "";
   }
}

class Main{
    public static fun Main() : Void
      {
		Fruit fruit = new Fruit(" ",10);
	}
}
```

error `Extends is invalid, can not find instance DefaultFruit (Test.mn:19:60) in class FruitApple (Test.mn:19:25)`

we can not extend the instance if the instance is private in parent class

```c#
class Fruit
{
    public Int Size;
    public String Name;
    public constructor(String name,Int size){
      Size = size;
      Name = name;
	
    }

   private instance DefaultFruit{
	Size = 10;
	Name = "";
   }
}
class Apple extends Fruit{
     public Int Size;
     public String Name;	

   public multi instance FruitApple(Int s,String x){
	Size = 10;
	Name = "";
   }
}

class Main{
    public static fun Main() : Void
      {
		Fruit fruit = new Fruit(" ",10);
		Fruit f = Fruit::DefaultFruit;
	}
}
```

error `Fruit<> does not have Instance that match the given arguments!`\

cannot find the instance because it is private in Fruit, if we want to use the private instance, we can only use in the Fruit CLass not in the MAin.











