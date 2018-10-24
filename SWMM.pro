#Author Caleb Amoa Buahin
#Email caleb.buahin@gmail.com
#Date 2018
#License GNU Lesser General Public License (see <http: //www.gnu.org/licenses/> for details).
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
#DEFINES += QT_NO_VERSION_TAGGING


CONFIG += c++11
CONFIG += debug_and_release
CONFIG += optimize_full

contains(DEFINES,SWMM_LIBRARY){
  TEMPLATE = lib
  message("Compiling as library")
} else {
  TEMPLATE = app
  CONFIG-=app_bundle
  message("Compiling as application")
}


PRECOMPILED_HEADER = ./include/stdafx.h

INCLUDEPATH += ./include \
               ./swmm5_iface/include \
               ./test/include

HEADERS +=./include/stdafx.h \
          ./include/dataexchangecache.h \
          ./test/include/swmmtestclass.h \
          ./include/couplingdatacache.h

SOURCES +=./src/stdafx.cpp \
          ./src/dataexchangecache.cpp \
          ./test/src/swmmtestdriver.cpp \
          ./test/src/swmmtestclass.cpp

message("SWMM Version: " $$VERSION)

INCLUDEPATH += ./$$VERSION/include

HEADERS += ./$$VERSION/include/consts.h \
           ./$$VERSION/include/datetime.h \
           ./$$VERSION/include/enums.h \
           ./$$VERSION/include/error.h \
           ./$$VERSION/include/exfil.h \
           ./$$VERSION/include/findroot.h \
           ./$$VERSION/include/funcs.h \
           ./$$VERSION/include/globals.h \
           ./$$VERSION/include/hash.h \
           ./$$VERSION/include/headers.h \
           ./$$VERSION/include/infil.h \
           ./$$VERSION/include/keywords.h \
           ./$$VERSION/include/lid.h \
           ./$$VERSION/include/macros.h \
           ./$$VERSION/include/mathexpr.h \
           ./$$VERSION/include/mempool.h \
           ./$$VERSION/include/objects.h \
           ./$$VERSION/include/odesolve.h \
           ./$$VERSION/include/swmm5.h \
           ./$$VERSION/include/text.h \
           ./$$VERSION/include/xsect.dat \
           ./swmm5_iface/include/swmm5_iface.h

SOURCES += ./$$VERSION/src/climate.c \
           ./$$VERSION/src/controls.c \
           ./$$VERSION/src/culvert.c \
           ./$$VERSION/src/datetime.c \
           ./$$VERSION/src/dwflow.c \
           ./$$VERSION/src/dynwave.c \
           ./$$VERSION/src/error.c \
           ./$$VERSION/src/exfil.c \
           ./$$VERSION/src/findroot.c \
           ./$$VERSION/src/flowrout.c \
           ./$$VERSION/src/forcmain.c \
           ./$$VERSION/src/gage.c \
           ./$$VERSION/src/gwater.c \
           ./$$VERSION/src/hash.c \
           ./$$VERSION/src/hotstart.c \
           ./$$VERSION/src/iface.c \
           ./$$VERSION/src/infil.c \
           ./$$VERSION/src/inflow.c \
           ./$$VERSION/src/input.c \
           ./$$VERSION/src/inputrpt.c \
           ./$$VERSION/src/keywords.c \
           ./$$VERSION/src/kinwave.c \
           ./$$VERSION/src/landuse.c \
           ./$$VERSION/src/lid.c \
           ./$$VERSION/src/lidproc.c \
           ./$$VERSION/src/link.c \
           ./$$VERSION/src/massbal.c \
           ./$$VERSION/src/mathexpr.c \
           ./$$VERSION/src/mempool.c \
           ./$$VERSION/src/node.c \
           ./$$VERSION/src/odesolve.c \
           ./$$VERSION/src/output.c \
           ./$$VERSION/src/project.c \
           ./$$VERSION/src/qualrout.c \
           ./$$VERSION/src/rain.c \
           ./$$VERSION/src/rdii.c \
           ./$$VERSION/src/report.c \
           ./$$VERSION/src/roadway.c \
           ./$$VERSION/src/routing.c \
           ./$$VERSION/src/runoff.c \
           ./$$VERSION/src/shape.c \
           ./$$VERSION/src/snow.c \
           ./$$VERSION/src/stats.c \
           ./$$VERSION/src/statsrpt.c \
           ./$$VERSION/src/subcatch.c \
           ./$$VERSION/src/surfqual.c \
           ./$$VERSION/src/swmm5.c \
           ./$$VERSION/src/table.c \
           ./$$VERSION/src/toposort.c \
           ./$$VERSION/src/transect.c \
           ./$$VERSION/src/treatmnt.c \
           ./$$VERSION/src/xsect.c \
           ./swmm5_iface/src/swmm5_iface.c



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

        LIBS += -L/usr/local/lib/ -lmpi

      message("MPI enabled")
    } else {
      message("MPI disabled")
    }
}

linux{

    INCLUDEPATH += /usr/include

    contains(DEFINES,USE_OPENMP){

        QMAKE_CFLAGS += -fopenmp
        QMAKE_LFLAGS += -fopenmp
        QMAKE_CXXFLAGS += -fopenmp

        message("OpenMP enabled")

     } else {

        message("OpenMP disabled")
     }
}

win32{

    contains(DEFINES,USE_OPENMP){

        QMAKE_CFLAGS += /openmp
        QMAKE_CXXFLAGS += /openmp

        message("OpenMP enabled")

     } else {

        message("OpenMP disabled")

     }

    #Windows vcpkg package manager installation path
    VCPKGDIR = C:/vcpkg/installed/x64-windows

    INCLUDEPATH += $${VCPKGDIR}/include \
                   $${VCPKGDIR}/include/gdal

    message ($$(VCPKGDIR))


    contains(DEFINES,USE_MPI){
       message("MPI enabled")

        CONFIG(debug, debug|release) {
            LIBS += -L$${VCPKGDIR}/debug/lib -lmsmpi
          } else {
            LIBS += -L$${VCPKGDIR}/lib -lmsmpi
        }

    } else {
      message("MPI disabled")
    }


    QMAKE_CXXFLAGS += /MP
    QMAKE_LFLAGS += /incremental /debug:fastlink
}

CONFIG(debug, debug|release) {

    win32 {
       QMAKE_CXXFLAGS += /MDd /O2
    }

    macx {
       QMAKE_CXXFLAGS += -O3
    }

    linux {
       QMAKE_CXXFLAGS += -O3
    }


   DESTDIR = ./build/debug
   OBJECTS_DIR = $$DESTDIR/.obj
   MOC_DIR = $$DESTDIR/.moc
   RCC_DIR = $$DESTDIR/.qrc
   UI_DIR = $$DESTDIR/.ui
}

CONFIG(release, debug|release) {


   win32 {
       QMAKE_CXXFLAGS+= /MD
   }

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
