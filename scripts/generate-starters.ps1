# Independently authored, deterministic beginner arrangements. No downloaded MIDI.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$folder=Join-Path $root 'assets/music'
New-Item -ItemType Directory -Force $folder | Out-Null
function Vlq([int]$n) { $a=[Collections.Generic.List[byte]]::new();$a.Add([byte]($n-band127));while(($n=$n-shr7)-gt0){$a.Insert(0,[byte](($n-band127)-bor128))};return $a.ToArray() }
function Be([int]$n,[int]$length){$a=[byte[]]::new($length);for($i=0;$i-lt$length;$i++){$a[$length-1-$i]=[byte](($n-shr(8*$i))-band255)};return $a}
function Track($notes,[int]$channel,[string]$name,[bool]$metadata){
 $events=[Collections.Generic.List[object]]::new();$events.Add(@{tick=0;order=0;bytes=[byte[]](255,3,$name.Length)+[Text.Encoding]::ASCII.GetBytes($name)})
 if($metadata){$events.Add(@{tick=0;order=0;bytes=[byte[]](255,81,3,10,44,43)});$events.Add(@{tick=0;order=0;bytes=[byte[]](255,88,4,4,2,24,8)});$events.Add(@{tick=0;order=0;bytes=[byte[]](255,89,2,0,0)})}
 $end=0;foreach($n in $notes){$events.Add(@{tick=$n[0];order=2;bytes=[byte[]]((144-bor$channel),$n[2],$n[3])});$events.Add(@{tick=($n[0]+$n[1]);order=1;bytes=[byte[]]((128-bor$channel),$n[2],0)});$end=[Math]::Max($end,$n[0]+$n[1])}
 $events.Add(@{tick=$end;order=3;bytes=[byte[]](255,47,0)})
 $stream=[IO.MemoryStream]::new();$last=0;foreach($event in ($events | Sort-Object tick,order)){[byte[]]$delta=Vlq ($event.tick-$last);$stream.Write($delta);$stream.Write([byte[]]$event.bytes);$last=$event.tick};[byte[]]$body=$stream.ToArray();$stream.Dispose();return [Text.Encoding]::ASCII.GetBytes('MTrk')+(Be $body.Length 4)+$body
}
$tunes=@(
 @{id='twinkle';title='Twinkle Twinkle';melody=@(60,60,67,67,69,69,67,65,65,64,64,62,62,60);source='Ah vous dirai-je maman, anonymous French melody first published 1761';roots=@(0,0,5,0)},
 @{id='mary';title='Mary Had a Little Lamb';melody=@(64,62,60,62,64,64,64,62,62,62,64,67,67,64,62,60,62,64,64,64,64,62,62,64,62,60);source='Traditional American nursery melody, nineteenth century';roots=@(0,0,7,0,0,7,0)},
 @{id='ode';title='Ode to Joy';melody=@(64,64,65,67,67,65,64,62,60,60,62,64,64,62,62,64,64,65,67,67,65,64,62,60,60,62,64,62,60,60);source='Ludwig van Beethoven, Symphony No. 9 Op. 125, 1824, opening theme';roots=@(0,7,0,7,0,7,0,0)},
 @{id='dan';title='Old Dan Tucker';melody=@(67,67,64,67,69,67,64,62,60,60,62,64,67,64,62,60);source='American folk melody published 1843; melody only, no lyrics';roots=@(0,5,7,0)},
 @{id='bach';title='BWV 846 — short excerpt';melody=@(60,64,67,72,76,67,72,76,60,62,69,74,77,69,74,77,59,62,67,74,77,67,74,77,60,64,67,72,76,67,72,76);source='Johann Sebastian Bach, Well-Tempered Clavier Book I, Prelude in C BWV 846, 1722; first four harmonies, original simplified revoicing';roots=@(0,2,7,0)}
)
$tunes+=@(
 @{id='major-scale';title='C Major Scale';melody=@(60,62,64,65,67,69,71,72,71,69,67,65,64,62,60,60);source='Original C major scale practice exercise';roots=@(0,5,7,0)},
 @{id='minor-scale';title='A Minor Scale';melody=@(57,59,60,62,64,65,67,69,67,65,64,62,60,59,57,57);source='Original A natural minor scale practice exercise';roots=@(9,2,4,9)},
 @{id='arpeggios';title='C Major Arpeggios';melody=@(60,64,67,72,67,64,60,64,65,69,72,77,74,71,67,60);source='Original major arpeggio practice exercise';roots=@(0,0,5,7)},
 @{id='intervals';title='Interval Steps';melody=@(60,62,60,64,60,65,60,67,60,69,60,71,60,72,67,60);source='Original ascending interval practice exercise';roots=@(0,0,0,0)},
 @{id='five-finger';title='Five Finger Patterns';melody=@(60,62,64,65,67,65,64,62,60,64,62,65,64,67,65,60);source='Original five finger practice exercise';roots=@(0,5,7,0)}
)
$manifest=@();foreach($t in $tunes){$melody=[Collections.Generic.List[object]]::new();$backing=[Collections.Generic.List[object]]::new();$chords=@();$duration=if($t.id-eq'bach'){240}else{480};$index=0;foreach($pitch in $t.melody){$melody.Add(@(($index*$duration),$duration,$pitch,82));$index++};$bars=[Math]::Ceiling($index*$duration/1920);for($bar=0;$bar-lt$bars;$bar++){$r=$t.roots[$bar%$t.roots.Length];$quality=if($t.id-eq'bach'-and$bar-eq1){@(0,3,7,10)}elseif($t.id-eq'minor-scale'){@(0,3,7)}elseif($r-eq7){@(0,4,7,10)}else{@(0,4,7)};$pitches=@($quality|ForEach-Object{48+$r+$_});$end=[Math]::Min(($bar+1)*1920,$index*$duration);$chords+=@{startTick=($bar*1920);endTick=$end;root=$r;pitches=$pitches;confidence=1.0;explicit=$true};foreach($pitch in $pitches){$backing.Add(@(($bar*1920),($end-$bar*1920),$pitch,55))}}
 [byte[]]$file=[Text.Encoding]::ASCII.GetBytes('MThd')+(Be 6 4)+[byte[]](0,1,0,2,1,224)+(Track $melody 0 'Practice melody' $true)+(Track $backing 1 'Piano backing' $false)
 [IO.File]::WriteAllBytes((Join-Path $folder ($t.id+'.mid')),$file)
 $manifest+=@{id=('starter-'+$t.id);file=($t.id+'.mid');title=$t.title;chords=$chords;provenance=@{source=$t.source;arrangement=($(if($t.source.StartsWith('Original')){'Sight Reading project, independently authored 2026-10-12'}else{'Sight Reading project, independently authored 2026-10-11'}));license='CC0-1.0';method=($(if($t.source.StartsWith('Original')){'Original educational note patterns, backing and MIDI encoding authored locally. No downloaded MIDI or reference-site assets used.'}else{'Melody entered from familiar public-domain musical themes; rhythm, voicing, backing and MIDI encoding authored locally. No reference-site code, MIDI or assets used.'}))}}
}
$manifest|ConvertTo-Json -Depth 12|Set-Content (Join-Path $folder 'manifest.json')
