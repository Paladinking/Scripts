@echo off
@setlocal
set in=%~1
set in=%in:\=\\%
set in='%in:'=\'%'
set out=%~2
set out=%out:\=\\%
set out='%out:'=\'%'
python -c "import zipfile; file = zipfile.ZipFile(%in%, 'r') ; file.extractall(%out%) ; file.close()"
