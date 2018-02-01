# SWMM
The EPA Stormwater Management Model refactored to enable multiple instances in the same program. Global variables have been encapsulated in a new structure so that independent instances on SWMM projects can be initialized and executed concurrently.


### Sample Code
---------------------------------------
``` C++

 Project *project = swmm_createProject();

 std::string inpuFileStr = "./../../examples/test1/test1.inp";
 char *inputFile = new char[inpuFileStr.size() + 1];
 std::strcpy(inputFile, inpuFileStr.c_str());

 std::string reportFileStr = "./../../examples/test1/test1_1.rpt";
 char *reportFile = new char[reportFileStr.size() + 1];
 std::strcpy(reportFile, reportFileStr.c_str());

 std::string outputFileStr = "./../../examples/test1/test1_1.out";
 char *outputFile = new char[outputFileStr.size() + 1];
 std::strcpy(outputFile, outputFileStr.c_str());

 swmm_run(project1, inputFile, reportFile, outputFile);

 delete[] inputFile;
 delete[] reportFile;
 delete[] outputFile;

 swmm_deleteProject(project);

```