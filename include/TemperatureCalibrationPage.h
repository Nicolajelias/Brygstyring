#pragma once
#include <Arduino.h>

const char TEMPERATURE_CALIBRATION_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="da"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PT100-kalibrering · Brygkontrol</title>
<style>
:root{color-scheme:dark;font:16px system-ui;color:#f7ead8;background:#100c08}body{max-width:900px;margin:auto;padding:1rem}a{color:#ffd27b}h1,h2{font-family:Georgia,serif}section{padding:1rem;margin:1rem 0;border:1px solid #453522;border-radius:1rem;background:#1d1711}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:1rem}label{display:block;margin-top:.7rem;color:#cdbb9f}input,select,button{font:inherit;padding:.65rem;border:1px solid #624a2b;border-radius:.5rem;background:#120e0a;color:#f7ead8;box-sizing:border-box;width:100%}button{background:#9b5d1c;margin-top:1rem;cursor:pointer}button:disabled{opacity:.5;cursor:not-allowed}.readings{line-height:1.8;color:#ffd27b}small{color:#cdbb9f}.hidden{display:none}#message{padding:.8rem 0;white-space:pre-line}.error{color:#ff9da4}</style>
</head><body><a href="/settings">← System</a><h1>PT100-kalibrering</h1>
<p>Kalibrér gryde og ventil hver for sig. Brug en stabil temperatur og et referencetermometer. Indtast altid den <strong>ukorrigerede</strong> måling, også ved gentagen kalibrering.</p>
<p>Ét punkt retter en konstant afvigelse. To punkter retter både afvigelse og hældning; brug et lavt og et højt punkt med mindst 10 °C afstand. Gas skal være slukket, og brygprocessen stoppet, når du gemmer. Pumpen må gerne cirkulere.</p>
<form id="calibration"><div id="channels"></div><button id="save" disabled>Gem kalibrering for begge sensorer</button></form>
<div id="message" role="status" aria-live="polite">Henter data…</div>
<p><small>Grænser: −20 til 125 °C, højst 10 °C punktkorrektion og offset; hældning 0,9–1,1. Vælg “Ingen” og gem for at nulstille en sensor. Kalibreringen er adskilt fra brygprofilens ventil-offset.</small></p>
<script>
const channels=[['pot','Gryde'],['valve','Ventil']];
const fields=[['measured1','Ukorrigeret måling, lavt punkt · °C'],['reference1','Reference, lavt punkt · °C'],['measured2','Ukorrigeret måling, højt punkt · °C'],['reference2','Reference, højt punkt · °C']];
const $=id=>document.getElementById(id);
for(const [id,name] of channels){const s=document.createElement('section');s.innerHTML=`<h2>${name}</h2><div class="readings" id="${id}Readings">—</div><label for="${id}Points">Kalibrering</label><select id="${id}Points" name="${id}Points"><option value="0">Ingen</option><option value="1">Ét punkt</option><option value="2">To punkter</option></select><div class="grid">${fields.map(([key,label])=>`<div id="${id}${key}Field"><label for="${id}${key}">${label}</label><input id="${id}${key}" name="${id}${key}" type="number" step="any" min="-20" max="125"></div>`).join('')}</div><p id="${id}Formula"></p>`;$('channels').appendChild(s);$(id+'Points').onchange=()=>showFields(id)}
function showFields(id){const n=Number($(id+'Points').value);for(const [key] of fields){const active=n>0&&(!key.endsWith('2')||n===2);$(id+key+'Field').classList.toggle('hidden',!active);$(id+key).disabled=!active;$(id+key).required=active}}
function temperature(v){return v===null?'Utilgængelig':Number(v).toFixed(3)+' °C'}
let loaded=false,saving=false;
async function refresh(){try{const r=await fetch('/api/calibration',{cache:'no-store'});if(!r.ok)throw Error('Kunne ikke hente kalibrering');const data=await r.json();for(const [id] of channels){const s=data[id];if(!loaded){$(id+'Points').value=s.points;for(const [key] of fields)$(id+key).value=s[key];showFields(id)}$(id+'Readings').textContent='Ukorrigeret: '+temperature(s.uncalibrated)+' · Kalibreret: '+temperature(s.value);$(id+'Formula').textContent='Gemt korrektion: temperatur × '+s.gain.toFixed(6)+' + '+s.offset.toFixed(4)+' °C'}$('save').disabled=!data.canSave||saving;if(!loaded){$('message').textContent=data.canSave?'Klar. Ændringer gemmes først, når du trykker Gem.':'Stop brygprocessen og sluk gas før du gemmer.';loaded=true}}catch(e){$('save').disabled=true;$('message').textContent=e.message;$('message').className='error'}finally{setTimeout(refresh,1500)}}
$('calibration').onsubmit=async e=>{e.preventDefault();saving=true;$('save').disabled=true;try{const r=await fetch('/api/calibration',{method:'POST',body:new URLSearchParams(new FormData(e.target))});const message=await r.text();if(!r.ok)throw Error(message);$('message').className='';$('message').textContent=message}catch(e){$('message').className='error';$('message').textContent=e.message}finally{saving=false}};
refresh();
</script></body></html>)HTML";
