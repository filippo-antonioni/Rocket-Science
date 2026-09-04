function flight = flight_sim(P)

% non uso ode45 perchè voglio vettore di integrazione a passo fisso quindi
% mi serve un metodo che non sia a passo variabile, es Eulero indietro.
% Avrei potuto usare RK che è più accurato ma visto che ci sono già
% incertezze maggiori e l'output sarebbe stato a passo variabile va bene
% anche Eulero indietro, oltre che ode45 sull esp non esiste pd 

dt = P.dt_fisica; 
n = round(P.t_max/ dt); 

% prealloco per risparmiare memoria tanto so quanto saranno lunghi i
% vettori 

t = zeros(n,1);
h = zeros(n,1);
v = zeros(n,1);
acc = zeros(n,1);

paracadute = 0; % non aperto
i_burnout=NaN; 
i_apogeo=NaN;


% mi serve solo per calcolare quanto propellente sto consumando 
I_tot = trapz(P.curva_spinta(:,1),P.curva_spinta(:,2));
I_i=0;


for i=1:n-1


 t(i+1) = t(i) + dt; % aggiorno tempo
  
 if t(i)<= P.t_burnout
    
     F_i = interp1(P.curva_spinta(:,1),P.curva_spinta(:,2),t(i),'linear',0); % calcolo istante per istante 
    
 else 
     F_i = 0; % sono oltre tempo di spinta del motore 
     if isnan(i_burnout)
        i_burnout= i; % salvo istante in cui ho burnout motore 
     end
 end

 % calcolo massa bruciata per trovare la massa effettiva 
 I_i = I_i + F_i*dt; 
 m_prop_i = P.massa_prop * (1- I_i/ I_tot);
 m_tot_i = P.massa_secca + m_prop_i; % aggiorno la massa totale ogni istante 


 % ricalcolo densità dell'aria 
 rho_i = P.rho0 * exp(-(h(i) + P.quota_rampa) / 8500); 
    

 % controllo Cd in base allo stato del paracadute 
 if paracadute 
    
     CdA = P.Cd_paracadute * P.area_paracadute; % paracadute aperto cambia il Cd 

 else 
    CdA = P.Cd * P.area;
 
 end 

% Calcolo il drag 

F_drag_i = -0.5 * rho_i *CdA* v(i)* abs(v(i)); 

acc(i) = (F_i + F_drag_i)/ m_tot_i - P.g;

%controllo se ho distacco dalla rampa 
if  h(i)<=0 && acc(i)<=0 && v(i) <=0
    
    acc(i)=0;
    v(i)=0;
else 

% Risolvo sistema di EDO con rk4, probabilemente non serve neanche
% un'accuratezza così elevata con un metodo numerico però meglio averla 

k1_h=v(i);
k1_v = calcola_acc(t(i),h(i),v(i),P,m_tot_i,CdA);

k2_h = v(i) + 0.5*dt*k1_v;
k2_v = calcola_acc(t(i)+ 0.5*dt,h(i)+ 0.5*dt*k1_h,v(i)+ 0.5*dt*k1_v,P,m_tot_i,CdA);

k3_h = v(i) + 0.5*dt*k2_v;
k3_v = calcola_acc(t(i) + 0.5*dt,h(i) + 0.5*dt*k2_h, v(i) + dt/2*k2_v,P,m_tot_i,CdA);

k4_h = v(i) +dt*k3_v;
k4_v = calcola_acc(t(i)+ dt, h(i)+ dt*k3_h, v(i) + dt*k3_v,P,m_tot_i,CdA);

h(i+1) = h(i) + dt/6*( k1_h + 2*k2_h + 2*k3_h + k4_h);
v(i+1) = v(i) + dt/6*(k1_v + 2*k2_v + 2*k3_v + k4_v);

end 


% Controllo apogeo 
if  isnan(i_apogeo) && v(i)>0 && v(i+1)<0 && h(i)>0 
    
    i_apogeo= i;
    paracadute= 1; %aperto

end


% atterraggio
if h(i+1)<0 && ~isnan(i_apogeo)  

    n=i+1;
    break
    
end




end


flight.t=t(1:n);
flight.h=h(1:n);
flight.v=v(1:n);
flight.acc=acc(1:n);
flight.t_burnout=t(i_burnout);
flight.t_apogeo=t(i_apogeo);
flight.h_apogeo=h(i_apogeo);
flight.v_max= max(flight.v);
flight.P=P;


fprintf('Simulazione: apogeo %.1f m a t=%.2f s | v_max %.1f m/s | burnout t=%.2f s\n', ...
        flight.h_apogeo, flight.t_apogeo, flight.v_max, flight.t_burnout);
end