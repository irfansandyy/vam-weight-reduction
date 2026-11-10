				
                #include<stdio.h>
#include<limits.h>
#define printf(...) ((void)0)
                

                int main(int argc, char **argv){
int allocation[128][128] = {};
                int c[128][128],// nilai cost
                i,j, //nilai row dan kolom
                min, m, n, b, d, c2, c1, p, q;
                int dem[128],sup[128],rf[128],cf[128]; long long sum=0;

				FILE *in;
				if(argc!=2) return 2; in=fopen(argv[1],"r"); if(!in) return 2;
	
				for(int once=0;once<1;once++)
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
                  { min=INT_MAX; p=-1; q=-1;
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
                                 if(p<0 || q<0) { fprintf(stdout,"{\"error\":\"original_min_sentinel_1000\",\"allocation\":null}\n"); return 0; }
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
                                 allocation[p][q] += c1;
                                 if(sup[p]<dem[q])
                                 { sum+=(long long)c[p][q]*sup[p];
                                                dem[q]-=sup[p];
                                                rf[p]=1;
                                                b--;
                                 }
                                 else
                                 if(sup[p]>dem[q])
                                 { sum=sum+(long long)c[p][q]*dem[q];
                                                sup[p]-=dem[q];
                                                cf[q]=1;
                                                d--;
                                 }
                                 else
                                 if(sup[p]==dem[q])
                                 { sum=sum+(long long)c[p][q]*sup[p];
                                                rf[p]=1;
                                                cf[q]=1;
                                                b--;
                                                d--;
                                 }

                                //printf("\n %d",sum);
                  }
                  printf("\n\n Biaya Minimumnya : %d\n\n",sum);
                  
                  fprintf(stdout,"{\"allocation\":[");
                  for(int ii=0;ii<m;ii++) {
                    if(ii) fprintf(stdout,","); fprintf(stdout,"[");
                    for(int jj=0;jj<n;jj++) fprintf(stdout,"%s%d",jj?",":"",allocation[ii][jj]);
                    fprintf(stdout,"]");
                  }
                  fprintf(stdout,"]}\n");

			    }
			}
