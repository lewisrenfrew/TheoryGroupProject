#include<stdio.h>
#include<math.h>
#include<iostream>
#include<cstdlib>
#include<fstream>
#include<time.h>
#include<stdlib.h>
#include <iomanip> 
using namespace std;

int main()
{
  //Initialising values
    double r1;      //Inner radius value
    double r2;      //Outer radius value
    int voltage;    //Voltage
    int X=200;      //Setting size of x dimension
    int Y=200;      //Setting size of y dimension
    double  array [X][Y];   //Creating 2d array
    ofstream myfile;        //Creating file
    myfile.open ("problem0.dat");  //Naming file to be written

    cout << "Enter value for inner radius";
    cout <<  endl;
    cin >> r1;    //Specifies value for inner radius
    cout << "Enter value for outer radius";
    cout <<  endl;
    cin >> r2;    //Specifies value for outer radius
    cout << "Enter value for voltage";
    cout <<  endl;
    cin >> voltage;  //Specifies value for voltage

    //X = radius2;
    //Y = radius2;

    for (int i = 0; i < X; i++) //Loops over x dimension
      {
	for (int j = 0; j < X; j++) //Loops over y dimension
	  {   
	    if (pow((i-r2),2) + pow((j-r2),2) <=  pow(r1,2.0) )
	        {
		    //If inside first radius sets value to zero
		    array[i][j] = 0;
		    
	        }
	    else if (pow((i-r2),2) + pow((j-r2),2) > pow(r2,2.0) )
	        {
	    	    array[i][j] = 0;
		}
	    else 
		{
		    array[i][j] = (voltage/log(r2/r1))*(log((pow((i-r2),2) + pow((j-r2),2))/r1));
		}
	    myfile << i <<setw(10) << j <<setw(20) << array[i][j];
	    myfile << "\n";
	  }
      }

    myfile.close();	   
	    
	      
    return 0;
    }
