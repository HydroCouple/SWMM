#Author Caleb Amoa Buahin
#Email caleb.buahin@gmail.com
#Date 2018
#License GNU General Public License (see <http: //www.gnu.org/licenses/> for details).
#EPA SWMM Model

TEMPLATE = lib
VERSION = 5.1.012
TARGET = SWMM
QT += testlib
QT -= gui

DEFINES += SWMM_LIBRARY
DEFINES += USE_OPENMP
DEFINES += USE_MPI
#DEFINES += SWMM_TEST

CONFIG += c++11
CONFIG += debug_and_release

contains(DEFINES,SWMM_LIBRARY){
  TEMPLATE = lib
  message("Compiling as library")
} else {
  TEMPLATE = app
  CONFIG-=app_bundle
  message("Compiling as application")
}

*msvc* { # visual studio spec filter
      QMAKE_CXXFLAGS += /MP /O2
  }


PRECOMPILED_HEADER = ./include/stdafx.h

INCLUDEPATH += ./include \
               ./swmm5_iface/include \
               ./test/include

HEADERS +=./include/stdafx.h \
          ./include/dataexchangecache.h \ 
          ./test/include/swmmtestclass.h

SOURCES +=./src/stdafx.cpp \
          ./src/dataexchangecache.cpp \
          ./test/src/swmmtestdriver.cpp \
          ./test/src/swmmtestclass.cpp

win32{
  INCLUDEPATH += $$PWD/graphviz/win32/include \
                 $$(MSMPI_INC)/
}

equals(VERSION,5.1.012){

    message("SWMM Version: " $$VERSION)

    INCLUDEPATH += ./5.1.012/include 
    
    HEADERS += ./5.1.012/include/consts.h \
               ./5.1.012/include/datetime.h \
               ./5.1.012/include/enums.h \
               ./5.1.012/include/error.h \
               ./5.1.012/include/exfil.h \
               ./5.1.012/include/findroot.h \
               ./5.1.012/include/funcs.h \
               ./5.1.012/include/globals.h \
               ./5.1.012/include/hash.h \
               ./5.1.012/include/headers.h \
               ./5.1.012/include/infil.h \
               ./5.1.012/include/keywords.h \
               ./5.1.012/include/lid.h \
               ./5.1.012/include/macros.h \
               ./5.1.012/include/mathexpr.h \
               ./5.1.012/include/mempool.h \
               ./5.1.012/include/objects.h \
               ./5.1.012/include/odesolve.h \
               ./5.1.012/include/swmm5.h \
               ./5.1.012/include/text.h \
               ./5.1.012/include/xsect.dat \
               ./swmm5_iface/include/swmm5_iface.h

    SOURCES += ./5.1.012/src/climate.c \
               ./5.1.012/src/controls.c \
               ./5.1.012/src/culvert.c \
               ./5.1.012/src/datetime.c \
               ./5.1.012/src/dwflow.c \
               ./5.1.012/src/dynwave.c \
               ./5.1.012/src/error.c \
               ./5.1.012/src/exfil.c \
               ./5.1.012/src/findroot.c \
               ./5.1.012/src/flowrout.c \
               ./5.1.012/src/forcmain.c \
               ./5.1.012/src/gage.c \
               ./5.1.012/src/gwater.c \
               ./5.1.012/src/hash.c \
               ./5.1.012/src/hotstart.c \
               ./5.1.012/src/iface.c \
               ./5.1.012/src/infil.c \
               ./5.1.012/src/inflow.c \
               ./5.1.012/src/input.c \
               ./5.1.012/src/inputrpt.c \
               ./5.1.012/src/keywords.c \
               ./5.1.012/src/kinwave.c \
               ./5.1.012/src/landuse.c \
               ./5.1.012/src/lid.c \
               ./5.1.012/src/lidproc.c \
               ./5.1.012/src/link.c \
               ./5.1.012/src/massbal.c \
               ./5.1.012/src/mathexpr.c \
               ./5.1.012/src/mempool.c \
               ./5.1.012/src/node.c \
               ./5.1.012/src/odesolve.c \
               ./5.1.012/src/output.c \
               ./5.1.012/src/project.c \
               ./5.1.012/src/qualrout.c \
               ./5.1.012/src/rain.c \
               ./5.1.012/src/rdii.c \
               ./5.1.012/src/report.c \
               ./5.1.012/src/roadway.c \
               ./5.1.012/src/routing.c \
               ./5.1.012/src/runoff.c \
               ./5.1.012/src/shape.c \
               ./5.1.012/src/snow.c \
               ./5.1.012/src/stats.c \
               ./5.1.012/src/statsrpt.c \
               ./5.1.012/src/subcatch.c \
               ./5.1.012/src/surfqual.c \
               ./5.1.012/src/swmm5.c \
               ./5.1.012/src/table.c \
               ./5.1.012/src/toposort.c \
               ./5.1.012/src/transect.c \
               ./5.1.012/src/treatmnt.c \
               ./5.1.012/src/xsect.c \
               ./swmm5_iface/src/swmm5_iface.c
}


macx{

    INCLUDEPATH += /usr/local \
                   /usr/local/include

    contains(DEFINES,USE_OPENMP){

        QMAKE_CC = /usr/local/opt/llvm/bin/clang
        QMAKE_CXX = /usr/local/opt/llvm/bin/clang++
        QMAKE_LINK = /usr/local/opt/llvm/bin/clang++

        QMAKE_CFLAGS+= -fopenmp
        QMAKE_LFLAGS+= -fopenmp
        QMAKE_CXXFLAGS+= -fopenmp

        INCLUDEPATH += /usr/local/opt/llvm/lib/clang/5.0.0/include
        LIBS += -L /usr/local/opt/llvm/lib -lomp

      message("OpenMP enabled")
     } else {
      message("OpenMP disabled")
     }

    contains(DEFINES,USE_MPI){

        QMAKE_CC = /usr/local/bin/mpicc
        QMAKE_CXX = /usr/local/bin/mpicxx
        QMAKE_LINK = /usr/local/bin/mpicxx

        QMAKE_CFLAGS += $$system(mpicc --showme:compile)
        QMAKE_CXXFLAGS += $$system(mpic++ --showme:compile)
        QMAKE_LFLAGS += $$system(mpic++ --showme:link)

        LIBS += -L/usr/local/lib/ -lmpi

      message("MPI enabled")
    } else {
      message("MPI disabled")
    }
}

linux{

INCLUDEPATH += /usr/include

    contains(DEFINES,UTAH_CHPC){

         INCLUDEPATH += /uufs/chpc.utah.edu/sys/installdir/hdf5/1.8.17-c7/include \
                        /uufs/chpc.utah.edu/sys/installdir/netcdf-c/4.3.3.1/include \
                        /uufs/chpc.utah.edu/sys/installdir/netcdf-cxx/4.3.0-c7/include \
                        ../hypre/build/include


         LIBS += -L/uufs/chpc.utah.edu/sys/installdir/hdf5/1.8.17-c7/lib -lhdf5 \
                 -L/uufs/chpc.utah.edu/sys/installdir/netcdf-cxx/4.3.0-c7/lib -lnetcdf_c++4 \
                 -L../hypre/build/lib -lHYPRE

         message("Compiling on CHPC")
     }

    contains(DEFINES,USE_OPENMP){

    QMAKE_CFLAGS += -fopenmp
    QMAKE_LFLAGS += -fopenmp
    QMAKE_CXXFLAGS += -fopenmp

    LIBS += -L/usr/lib/x86_64-linux-gnu -lgomp

      message("OpenMP enabled")
     } else {
      message("OpenMP disabled")
     }
}

win32{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CFLAGS += /openmp
        QMAKE_CXXFLAGS += /openmp
        QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CXXFLAGS /MD
        QMAKE_CXXFLAGS_DEBUG = $$QMAKE_CXXFLAGS /MDd
        message("OpenMP enabled")
     } else {

      message("OpenMP disabled")
     }

    contains(DEFINES,USE_MPI){
       LIBS += -L$$(MSMPI_LIB64)/ -lmsmpi
       message("MPI enabled")
     } else {
      message("MPI disabled")
     }
}

CONFIG(debug, debug|release) {

   DESTDIR = ./build/debug
   OBJECTS_DIR = $$DESTDIR/.obj
   MOC_DIR = $$DESTDIR/.moc
   RCC_DIR = $$DESTDIR/.qrc
   UI_DIR = $$DESTDIR/.ui
}

CONFIG(release, debug|release) {

    RELEASE_EXTRAS = ./build/release 
    OBJECTS_DIR = $$RELEASE_EXTRAS/.obj
    MOC_DIR = $$RELEASE_EXTRAS/.moc
    RCC_DIR = $$RELEASE_EXTRAS/.qrc
    UI_DIR = $$RELEASE_EXTRAS/.ui

     contains(DEFINES,SWMM_LIBRARY){
         #MacOS
         macx{
             DESTDIR = lib/macx
          }

         #Linux
         linux{
             DESTDIR = lib/linux
          }

         #Windows
         win32{
             DESTDIR = lib/win32
          }
     } else {
         #MacOS
         macx{
             DESTDIR = bin/macx
          }

         #Linux
         linux{
             DESTDIR = bin/linux
          }

         #Windows
         win32{
             DESTDIR = bin/win32
          }
     }
}
