@echo off
:: $Id: debug_setup.bat,v 1.1 2002/03/25 06:30:12 rossta Exp $

mkdir Debug
cd Debug
copy ..\..\data\mime.types .
mkdir OpenFT
cd OpenFT
copy ..\..\..\data\OpenFT\nodes . 
copy ..\..\..\data\OpenFT\*.gif . 
copy ..\..\..\data\OpenFT\*.jpg . 
copy ..\..\..\data\OpenFT\*.css . 
