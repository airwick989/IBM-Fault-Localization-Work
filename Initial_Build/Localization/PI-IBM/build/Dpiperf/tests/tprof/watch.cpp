#include <iostream>

int One()
{
   int value = 0;
   for (int32_t i = 0; i < 10000; i++)
   {
      for (int32_t j = 0; j < 50000; j++)
      {
         if (j % 2)
         {
            value += 3;
         }
         else
         {
            value -= 1;
         }
      }
   }
   return value;
}

int Two()
{
   int value = 0;
   for (int32_t i = 0; i < 20000; i++)
   {
      for (int32_t j = 0; j < 50000; j++)
      {
         if (j % 2)
         {
            value += 3;
         }
         else
         {
            value -= 1;
         }
      }
   }
   return value;
}

int Four()
{
   int value = 0;
   for (int32_t i = 0; i < 40000; i++)
   {
      for (int32_t j = 0; j < 50000; j++)
      {
         if (j % 2)
         {
            value += 3;
         }
         else
         {
            value -= 1;
         }
      }
   }
   return value;
}

int Eight()
{
   int value = 0;
   for (int32_t i = 0; i < 80000; i++)
   {
      for (int32_t j = 0; j < 50000; j++)
      {
         if (j % 2)
         {
            value += 3;
         }
         else
         {
            value -= 1;
         }
      }
   }
   return value;
}

int Sixteen()
{
   int value = 0;
   for (int32_t i = 0; i < 160000; i++)
   {
      for (int32_t j = 0; j < 50000; j++)
      {
         if (j % 2)
         {
            value += 3;
         }
         else
         {
            value -= 1;
         }
      }
   }
   return value;
}



int main(void)
{
   do 
   {
      std::cout << "Press the ENTER key to start: ";
   }
   while (std::cin.get() != '\n');

   std::cout << "One\n";
   One(); 
   std::cout << "Two\n";
   Two(); 
   std::cout << "Four\n";
   Four(); 
   std::cout << "Eight\n";
   Eight(); 
   std::cout << "Sixteen\n";
   Sixteen(); 

   return 0;
}


