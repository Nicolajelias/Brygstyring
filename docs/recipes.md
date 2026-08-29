# Opskrifter og humlealarmer

`/recipes` indeholder op til 12 navngivne opskrifter. En opskrift omfatter hele
brygprofilen: mæskning, udmæskning, kogning, temperaturregulering og op til otte
humletilsætninger.

Humletider angives som minutter **før kogeslut**. En tilsætning ved 60 minutter
alarmerer ved starten af en 60-minutters kogning; en tilsætning ved 0 minutter
alarmerer ved flameout. Alarmen giver buzzer og dashboard-advarsel og kvitteres
med enhedens knap eller knappen i dashboardet.

Opskriftslageret bruger SPIFFS, CRC-kontrol og atomisk temp-/backup-udskiftning.
En ugyldig primær fil bevares som `/recipes.corrupt`. Valgt opskrift gendannes
efter genstart. Opskrifter kan kun vælges, ændres eller slettes, mens processen
er stoppet.
