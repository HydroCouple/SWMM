# SWMM
The EPA Stormwater Management Model refactored to enable multiple instances in the same program. Global variables have been encapsulated in a new structure so that independent instances on SWMM projects can be initialized and executed concurrently.


### Sample Code
---------------------------------------
``` C++

Project *project1 = nullptr;
Project *project2 = nullptr;
 
swmm_createProject(&project1);
swmm_createProject(&project2);

std::string inpuFileStr = "./../../examples/test1/test1.inp";
char *inputFile1 = new char[inpuFileStr.size() + 1];
std::strcpy(inputFile, inpuFileStr.c_str());

std::string reportFileStr1 = "./../../examples/test1/test1_1.rpt";
char *reportFile1 = new char[reportFileStr1.size() + 1];
std::strcpy(reportFile1, reportFileStr1.c_str());

std::string reportFileStr2 = "./../../examples/test1/test1_2.rpt";
char *reportFile2 = new char[reportFileStr2.size() + 1];
std::strcpy(reportFile2, reportFileStr2.c_str());

std::string outputFileStr1 = "./../../examples/test1/test1_1.out";
char *outputFile1 = new char[outputFileStr1.size() + 1];
std::strcpy(outputFile1, outputFileStr1.c_str());

std::string outputFileStr2 = "./../../examples/test1/test1_2.out";
char *outputFile2 = new char[outputFileStr2.size() + 1];
std::strcpy(outputFile2, outputFileStr2.c_str());

#pragma omp parallel sections
{
  #pragma omp section
  {
    swmm_run(project1, inputFile, reportFile1, outputFile1);
  }
  
  #pragma omp section
  {
    swmm_run(project2, inputFile, reportFile2, outputFile2);
  }
}

delete[] inputFile;
delete[] reportFile1;
delete[] reportFile2;
delete[] outputFile1;
delete[] outputFile2;

swmm_deleteProject(project1);
swmm_deleteProject(project2);

```