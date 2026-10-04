#!/bin/bash
echo "🚀 Lanzando tests y calculando cobertura..."
ceedling clobber gcov:all
# Solo el codigo de aplicacion (App/Src). Fuera mocks, runners, Unity,
# CMock y los propios test_*.c, que no son codigo del nodo.
gcovr --root .. --filter '\.\./App/Src/' --html-details -o build/artifacts/gcov/GcovCoverageResults.html build/
gcovr --root .. --filter '\.\./App/Src/' build/
open build/artifacts/gcov/GcovCoverageResults.html
