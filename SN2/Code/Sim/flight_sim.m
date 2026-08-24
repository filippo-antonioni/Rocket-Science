function flight = flight_sim(P)

% non uso ode45 perchè voglio vettore di integrazione a passo fisso quindi
% mi serve un metodo che non sia a passo variabile, es Eulero indietro.
% Avrei potuto usare RK che è più accurato ma visto che ci sono già
% incertezze maggiori e l'output sarebbe stato a passo variabile va bene
% anche Eulero indietro, oltre che ode45 sull esp non esiste pd 

dt = P.dt_fisica; 
N = round(P.t_max/ dt); 

% prealloco per risparmiare memoria tanto so quanto saranno lunghi i
% vettori 

t = zeros(n,1);
h = zeros(n,1);
v = zeros(n,1);
acc = zeros(n,1);

paracadute = 0; % non aperto

m_prop= P.massa_prop;






end