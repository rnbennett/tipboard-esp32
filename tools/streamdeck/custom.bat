@echo off
powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/status -Method Put -ContentType 'application/json' -Body '{\"mode\":5}'"
