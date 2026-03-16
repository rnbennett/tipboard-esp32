@echo off
powershell -WindowStyle Hidden -Command "Invoke-RestMethod -Uri http://10.0.0.30/api/timer/start -Method Post -ContentType 'application/json' -Body '{\"type\":\"pomodoro\",\"work_min\":25}'"
