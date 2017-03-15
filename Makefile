#----------------------------------------------------------------------------
OUT_NAME    := tick
OUT_DIR     := .
CLEAN_FILES := "$(OUT_DIR)/$(OUT_NAME).exe" "data.txt" a.out
#----------------------------------------------------------------------------
# 1-st way to select source files
#SRCS := tick.c sgpio.c

#HDRS := sgpio.h

# 2-nd way to select source files
SRC_DIRS := .
HDR_DIRS := .
#----------------------------------------------------------------------------
#DEFS   :=
#OPTIM  := -g -O0
OPTIM   := -Os
WARN    := -Wall
CFLAGS  := $(WARN) $(OPTIM) $(DEFS) $(CFLAGS) -pipe
LDFLAGS := -lm -lrt -ldl -lpthread $(LDFLAGS)
PREFIX  := /opt
#----------------------------------------------------------------------------
#_AS  := @as
#_CC  := @gcc
#_CXX := @g++
#_LD  := @gcc

#_CC  := @clang
#_CXX := @clang++
#_LD  := @clang
#----------------------------------------------------------------------------
include Makefile.skel
#----------------------------------------------------------------------------

