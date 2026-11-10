				#include<conio.h>
                #include<stdio.h>
                #include"windows.h"

                int main(){
                int c[100][100],// nilai cost
                i,j, //nilai row dan kolom
                min, m, n, b, d, c2, c1, p, q;
                int dem[100],sup[100],rf[100],cf[100],sum=0;

				FILE *in;
				in=fopen("test1.txt","r");
	
				while(!feof(in))
				{
					fscanf(in,"%d", &m);	
					fscanf(in,"%d", &n);
					
//                  printf("\nMasukan Banyaknya Row: ");
//                  scanf("%d",&m);
//                  printf("\nMasukan Banyaknya Kolom: ");
//                  scanf("%d",&n);


//                  printf("\nMasukkan Cost: \n");         //matrix
                    for(i=0;i<m;i++){
                        for(j=0; j<n; j++){
                        fscanf(in,"%d",&c[i][j]);
						}
                    }
                
                  
//                  printf("\nMasukkan Supply: \n");
                  for(i=0; i<m; i++){ //looping sebanyak row
                    fscanf(in,"%d",&sup[i]);
                  }
                  
//                  printf("\nMasukkan Demand: \n");
                  for(i=0; i<n; i++){//looping sebanyak kolom yang diinput
                    fscanf(in,"%d",&dem[i]);
                  }

                  printf("\nMatrix:\n");
                  for(i=0; i<m; i++){
                                 for(j=0; j<n; j++)
                                                  printf(" %d | ",c[i][j]);
                                                  printf("%d  ",sup[i]);
                                                  printf("\n");
                                 }
                  for(j=0; j<n; j++)
                                printf("%d  ",dem[j]);
                  for(i=0; i<m; i++)
                                rf[i]=0;
                  for(i=0; i<n; i++)
                                cf[i]=0;
                  b=m;
                  d=n;
                  while(b>0 && d>0)
                  { min=1000;
                                 for(i=0; i<m; i++)
                                 { if(rf[i]!=1)
                                                { for(j=0; j<n; j++)
                                 { if(cf[j]!=1)
                                                { if(min>c[i][j])
                                                  { min=c[i][j];
                                                                  p=i;
                                                                  q=j;
                                                  }
                                                }
                                 }
                                                }
                                 }
                                 if(sup[p] < dem[q])
                                  c1=sup[p];
                                 else
                                  c1=dem[q];
                                 for(i=0; i<m; i++)
                                 { if(rf[i]!=1)
                                                { for(j=0; j<n; j++)
                                 { if(cf[j]!=1)
                                                { if(min==c[i][j])
                                                  { if(sup[i]<dem[j])
                                                                  c2=sup[i];
                                                                 else
                                                                  c2=dem[j];
                                                                 if(c2>c1)
                                                                 { c1=c2;
                                                  p=i;
                                                  q=j;
                                                                 }
                                                  }
                                                }
                                 }
                                                }
                 }
                                 //printf("\n  %d %d %d ",min,p,q);
                                 if(sup[p]<dem[q])
                                 { sum+=c[p][q]*sup[p];
                                                dem[q]-=sup[p];
                                                rf[p]=1;
                                                b--;
                                 }
                                 else
                                 if(sup[p]>dem[q])
                                 { sum=sum+c[p][q]*dem[q];
                                                sup[p]-=dem[q];
                                                cf[q]=1;
                                                d--;
                                 }
                                 else
                                 if(sup[p]==dem[q])
                                 { sum=sum+c[p][q]*sup[p];
                                                rf[p]=1;
                                                cf[q]=1;
                                                b--;
                                                d--;
                                 }

                                //printf("\n %d",sum);
                  }
                  printf("\n\n Biaya Minimumnya : %d\n\n",sum);
                  //getch();
			    }
			}
