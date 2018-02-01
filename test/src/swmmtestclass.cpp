/*!
 * \file swmmtestclass.cpp
 * \author Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version 5.1.012
 * \description
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

#include <omp.h>

#include "swmm5.h"
#include "headers.h"
#include "swmmtestclass.h"

void SWMMTestClass::init()
{

}

void SWMMTestClass::concurrentSameInput()
{
  QBENCHMARK
  {
    {
      Project *project1 = createProjectObject();
      Project *project2 = createProjectObject();

#pragma omp parallel sections
      {
#pragma omp section
        {
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
        }

#pragma omp section
        {
          std::string inpuFileStr = "./../../examples/test1/test1.inp";
          char *inputFile = new char[inpuFileStr.size() + 1];
          std::strcpy(inputFile, inpuFileStr.c_str());

          std::string reportFileStr = "./../../examples/test1/test1_2.rpt";
          char *reportFile = new char[reportFileStr.size() + 1];
          std::strcpy(reportFile, reportFileStr.c_str());

          std::string outputFileStr = "./../../examples/test1/test1_2.out";
          char *outputFile = new char[outputFileStr.size() + 1];
          std::strcpy(outputFile, outputFileStr.c_str());

          swmm_run(project2, inputFile, reportFile, outputFile);

          delete[] inputFile;
          delete[] reportFile;
          delete[] outputFile;
        }
      }

      QString error1(project1->ErrorMsg);
      QString error2(project2->ErrorMsg);

      QVERIFY2(project1->ErrorCode == 0, error1.toStdString().c_str());
      QVERIFY2(project2->ErrorCode == 0, error2.toStdString().c_str());

      deleteProjectObject(project1);
      deleteProjectObject(project2);
    }
  }
}

void SWMMTestClass::concurrentDifferentInputs()
{

}

void SWMMTestClass::cleanup()
{

}
