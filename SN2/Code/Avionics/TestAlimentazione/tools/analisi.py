"""Analisi del solo test POWER_TEST: integrita, intervalli e finestre del servo."""
import csv
import html
import json
import math
import pathlib
import statistics
import log_io_v4 as logio


def describe(values):
    values=[v for v in values if math.isfinite(v)]
    return (dict(n=len(values),mean=statistics.mean(values),min=min(values),max=max(values))
            if values else None)


def analyse(binary, folder, result=None):
    meta, rows=logio.read_log(binary)
    if (meta['version'],meta['scenario'],meta['log_mode'],meta['core_mode'],meta['clock_mode'])!=(3,101,3,2,1):
        raise ValueError('File diverso dal test alimentazione: conservato, nessuna cancellazione')
    if not rows:
        raise ValueError('File senza campioni: possibile reset prima del salvataggio; dati PSRAM non recuperabili dopo spegnimento')
    if rows[0]['sequenza']!=0:
        raise ValueError('Manca inizio registrazione')
    events=[];previous=0
    for i,r in enumerate(rows):
        if r['fase']!='PAD' or r['flags']&(4|16):
            raise ValueError('Stato/deploy/simulazione inattesi')
        if r['dt_us']!=r['dt_reale_us']:
            raise ValueError('Intervallo matematico/reale incoerente')
        if i and r['wall_us']-rows[i-1]['wall_us']!=r['dt_reale_us']:
            raise ValueError('Tempi non continui')
        if not i and r['wall_us']!=r['dt_us']:
            raise ValueError('Primo timestamp incoerente')
        reason=r['motivo']
        if reason!=previous:
            if reason!=previous+1 or reason>3: raise ValueError('Sequenza comandi servo incoerente')
            events.append(dict(index=reason,time_s=r['wall_us']/1e6,command='APRI' if reason in (1,3) else 'CHIUDI'))
            previous=reason
        expected_open=reason in (1,3)
        if bool(r['flags']&8)!=expected_open:
            raise ValueError('Flag PWM e comando incoerenti')
        r['fase']='TEST_ALIMENTAZIONE'
        r['imu_valida']=int(bool(r['flags']&1));r['baro_nuovo']=int(bool(r['flags']&2))
        r['servo_aperto_comandato']=int(expected_open)
        for axis in ['ax','ay','az']:
            raw=r[axis+'_raw'];r[axis+'_ms2']=raw/2048*9.80665 if raw!=-32768 else math.nan
    if result and len(rows)!=int(result['rows']):
        raise ValueError('Numero record diverso dal riepilogo: conservare entrambi')
    duration=rows[-1]['wall_us']/1e6
    windows=[]
    for event in events:
        t=event['time_s']
        for label,lo,hi in [('prima',t-2,t),('dopo',t,t+2)]:
            q=[r for r in rows if lo<=r['wall_us']/1e6<hi]
            windows.append(dict(event=event['index'],command=event['command'],window=label,
                start_s=lo,end_s=hi,samples=len(q),max_dt_us=max((r['dt_us'] for r in q),default=0),
                imu_invalid=sum(not r['imu_valida'] for r in q),
                baro_fresh=sum(r['baro_nuovo'] for r in q)))
    imu_times=[r['wall_us'] for r in rows if r['imu_valida']]
    baro_times=[r['wall_us'] for r in rows if r['baro_nuovo']]
    complete=bool(result and result.get('complete')==1 and result.get('log')=='PASS' and
                  result.get('ram_pending')==0 and result.get('flash_ops_capture')==0 and
                  result.get('servo')=='PASS' and len(events)==3 and
                  abs(rows[-1]['wall_us']/1000-meta['duration_ms'])<=30)
    data=dict(crc_and_sequence='PASS',complete=complete,voltage_measured=False,
        samples=len(rows),last_sample_s=duration,dt_us=describe([r['dt_us'] for r in rows]),
        imu_invalid=sum(not r['imu_valida'] for r in rows),
        max_valid_imu_gap_us=max((b-a for a,b in zip(imu_times,imu_times[1:])),default=0),
        max_fresh_baro_gap_us=max((b-a for a,b in zip(baro_times,baro_times[1:])),default=0),
        numerical=sum(bool(r['flags']&32) for r in rows),events=events,servo_windows=windows,
        firmware_result=result)
    folder=pathlib.Path(folder)
    with (folder/'campioni.csv').open('x',newline='',encoding='utf8') as out:
        writer=csv.DictWriter(out,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    (folder/'analisi.json').write_text(json.dumps(data,indent=2,allow_nan=False),encoding='utf8')
    body='<h1>Test alimentazione, sensori e servo</h1>'
    body+='<p>Il firmware non misura la tensione. Il comando PWM non conferma il movimento fisico del servo. '
    body+='Un log completo e tempi regolari non escludono brevi cali dei 5 V.</p>'
    body+=f'<p>Integrita e sequenze PASS; {len(rows)} campioni; test completo: {complete}; '
    body+=f'intervallo massimo {data["dt_us"]["max"]} us; IMU invalide {data["imu_invalid"]}.</p>'
    body+='<h2>Finestre di due secondi attorno ai comandi</h2><table><tr><th>Comando</th><th>Finestra</th><th>Max dt us</th><th>IMU invalide</th></tr>'
    for w in windows:
        body+=f'<tr><td>{w["event"]} {w["command"]}</td><td>{w["window"]}</td><td>{w["max_dt_us"]}</td><td>{w["imu_invalid"]}</td></tr>'
    body+='</table>'
    for fields,title,limit in [
        ([('dt_reale_us','#7a0177','Intervallo reale')],'Intervallo campioni (us)',20000),
        ([('servo_aperto_comandato','#d95f02','Servo aperto comandato')],'Timer servo: 60 s APRI, 90 s CHIUDI, 120 s APRI',None),
        ([('quota_baro_m','#2166ac','Barometro'),('h_m','#b2182b','Kalman indicativo')],'Quota (m); Kalman valido solo con orientamento previsto',None),
        ([('ax_ms2','#b2182b','X'),('ay_ms2','#238b45','Y'),('az_ms2','#2166ac','Z')],'Accelerazione specifica (m/s2)',None),
        ([('imu_valida','#238b45','IMU valida'),('baro_nuovo','#2166ac','Baro nuovo')],'Disponibilita sensori',None)]:
        body+=logio.svg_chart(rows,fields,title,True,limit)
    body+='<h2>Riepilogo</h2><pre>'+html.escape(json.dumps(data,indent=2))+'</pre>'
    page='<!doctype html><html lang="it"><meta charset="utf-8"><title>Test alimentazione</title><style>body{font:16px system-ui;max-width:1000px;margin:25px auto;padding:15px}svg{width:100%}td,th{border:1px solid #ccc;padding:6px}table{border-collapse:collapse}pre{white-space:pre-wrap}</style>'+body+'</html>'
    (folder/'grafici.html').write_text(page,encoding='utf8')
    return data
