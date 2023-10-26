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
    public multi instance OtherFruit(Int weight)
    {
       Weight = weight;
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

### 4. Default expression with instance assignment 

```c#
 public instance DefaultFruit
    {
       FruitName = "apple";
    }
    public multi instance OtherFruit(Int size)
    {
       FruitName = Fruit::DefaultFruit;
       Size = size;
    }
```

### 5. Using Instance to initialize field 

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

### 6. need initial all the field 

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
public instance DefaultFruit
    {
       FruitName = "apple";
    }
public multi instance OtherFruit(Int size)
    {
       f = Fruit::DefaultFruit(1,2,3);
       Size = size;
    }
```



### 2. Assignment Field is not in the class  Field

```c#
class Fruit
{
    public Int Size;
    public String FruitName;
    public Fruit f;
    public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = new Fruit("banana",5);
      Size = Maximize(10);
    }
    public instance DefaultFruit
    {
       FruitName = "apple";
    }
    public multi instance OtherFruit(Int size)
    {
       //  here 'size' should be in the class field which here is Size, FruitName , f
       size = Size;
    }
    public static fun Maximize(Int x) : Int
	{		
		return x;
	}
}
```



### 3. Do not have the Instance 

```c#
 public constructor(String name,Int size){
      Size = size;
      FruitName = name;
      f = Fruit::AppleFruit(1,2,3);
      t = new Tree(f);
    }
```



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
public instance DefaultFruit
    {
       f = this;
       FruitName = 1; 
    }
```









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



### Super class

finished

```C#
class Apple extends Tree{
     public Int Size;
     public String Name;	

   public multi instance FruitApple(Int s,String x) extends DefaultFruit{
	Size = 10;
	Name = "";
   }
}
class Tree{

}
```

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







