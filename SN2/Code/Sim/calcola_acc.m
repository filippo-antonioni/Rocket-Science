function [a] = calcola_acc(t,h,v,P,m,CdA)

F = interp1(P.curva_spinta(:,1),P.curva_spinta(:,2),t,'linear',0);


rho = P.rho0 * exp(-(h + P.quota_rampa) / 8500);
D = -0.5 * rho *CdA* v* abs(v); 
    
    
a = (F + D) / m - P.g;


end