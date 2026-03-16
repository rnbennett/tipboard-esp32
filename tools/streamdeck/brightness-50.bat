@echo off
powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/brightness -Method Put -ContentType 'application/json' -Body '{\"value\":50}'"
