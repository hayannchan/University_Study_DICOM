#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include "opencv4/opencv2/opencv.hpp"
#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/core/types.hpp>
#include <opencv4/opencv2/highgui/highgui.hpp>
#include "opencv4/opencv2/imgproc.hpp"
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmimgle/dcmimage.h"
#include <X11/Xlib.h>
#include <filesystem>
#include "gnuplot_i.hpp"
#define _CRT_SECURE_NO_WARNINGS
#define OPENCV_TRAITS_ENABLE_DEPRECATED
#define R 1.5
#define windowName "suspected"

using namespace std;
using namespace cv;
namespace stdfs = std::filesystem;

struct Point3D
{
    int x,y,z;
    Point3D (int x,int y,int z){
        this->x=x;
        this->y=y;
        this->z=z;
    }
    Point3D () {
        this->x=0;
        this->y=0;
        this->z=0;
    }
    bool operator==(const Point3D &other) const{
        if (this->x == other.x && this->y == other.y && this->z == other.z) return true;
        return false;
    }
    bool operator!=(const Point3D &other) const{
        if (this->x != other.x || this->y != other.y || this->z != other.z) return true;
        return false;
    }
    bool operator<(const Point3D &other) const {
        if(this->z<other.z) return true;
        if(this->z==other.z && this->x<other.x) return true;
        if(this->z==other.z && this->x==other.x && this->y<other.y) return true;
        return false;
    }
};

class DicomData
{
public:
    int*** PixelData;
    int width = 0, height = 0, imgQuan = 0;
    Point3D start = Point3D(-1,-1,-1);
    set<Point3D> points=set<Point3D>();
    set<Point3D> pointsCom = set<Point3D>();
    set<Point3D> pointsSep1 = set<Point3D>();
    set<Point3D> pointsSep2 = set<Point3D>();
    set<Point3D> centersCom = set<Point3D>();
    set<Point3D> centersSep1 = set<Point3D>();
    set<Point3D> centersSep2 = set<Point3D>();
    int separationLayer = 0;
    vector<double> radsCom = vector<double>();
    vector<double> radsSep1 = vector<double>();
    vector<double> radsSep2 = vector<double>();
    int MIN_VAL = -100;
    int MAX_VAL = 300;
    int MAX_CONTRAST = 20;
    QString name = "NONAME";
    bool changed = false;
    double thickness = 1;
    double stenos = 0;
    int stenosLayers[2] = {-1, -1};
    set<Point3D> stenosMinPoints = set<Point3D>();
    set<Point3D> stenosMaxPoints = set<Point3D>();

    DicomData(DicomImage** imagesFiles, int imgQuan) {
        this->PixelData=getPixelData(imagesFiles,imgQuan);
        this->start.z = imgQuan-1;
    }

    void setStart (int x, int y, int z) {
        if ((z==0 && !changed) || (z==imgQuan-1 && changed)) {
            int*** newPixelData = new int**[imgQuan];
            for (int i=0;i<imgQuan;i++){
                newPixelData[i] = new int*[height];
                for (int k = 0;k < height; k++) {
                    newPixelData[i][k] = new int[width];
                    for (int l = 0; l<width;l++){
                        newPixelData[i][k][l] = PixelData[imgQuan-1-i][k][l];
                    }
                }

            }
            this->PixelData = newPixelData;
            changed = !changed;
            qDebug() << "changed";
        }
        this->start = Point3D(x,y,z);
        if (z == 0) {
            this->start = Point3D(x,y,imgQuan-1);
        }
    }

    void setStart (Point3D start) {
        setStart(start.x,start.y,start.z);
    }

    void reset() {
        points.clear();
        pointsCom.clear();
        pointsSep1.clear();
        pointsSep2.clear();
        centersCom.clear();
        centersSep1.clear();
        centersSep2.clear();
        separationLayer = 0;
        radsCom.clear();
        radsSep1.clear();
        radsSep2.clear();
        stenos = 0;
        stenosLayers[0] = -1;
        stenosLayers[1] = -1;
        stenosMinPoints.clear();
        stenosMaxPoints.clear();
        separationLayer = 0;
    }

    int*** getPixelData(DicomImage** imagesFiles, int imgQuan) {
        int*** result = new int**[imgQuan];
        this->imgQuan = imgQuan;
        for (int i=0;i<imgQuan;i++){
            const DiPixel *DiPixelData = (imagesFiles[i]->getInterData());
            Sint16* pixelData = (Sint16 *)DiPixelData->getData();
            unsigned long w = imagesFiles[i]->getWidth();
            this->width = w;
            unsigned long h = imagesFiles[i]->getHeight();
            this->height = h;
            result[i] = new int*[h];
            for (int j = 0;j < h;j++) {
                result[i][j] = new int[w];
                for (int k=0;k < w;k++){
                    result[i][j][k]=pixelData[(j*w+k)];
                }
            }
        }
        if (changed) std::reverse(result+0,result+imgQuan);
        return result;
    }

    int getValue(Point3D p){
        return PixelData[p.z][p.x][p.y];
    }
    bool checkPoint(Point3D cur, Point3D prev){
        if (cur.x>=width || cur.x<0 || cur.y>=height || cur.y<0 || cur.z>=imgQuan || cur.z<0) return false;
        if (getValue(prev)-getValue(cur)>MAX_CONTRAST) return false;
        if (getValue(cur)<MIN_VAL || getValue(cur)>MAX_VAL) return false;
        if (points.find(cur)!=points.cend()) return false;
        return true;
    }

    void setPoints() {
        if (start.x==-1) return;
        set<Point3D> toAdd = set<Point3D>();
        toAdd.insert(start);
        while (!toAdd.empty()){
            Point3D cur = *toAdd.begin();
            toAdd.erase(cur);
            points.insert(cur);
            if (checkPoint(Point3D(cur.x+1,cur.y,cur.z),cur) && toAdd.find(Point3D(cur.x+1,cur.y,cur.z)) == toAdd.cend()) toAdd.insert(Point3D(cur.x+1,cur.y,cur.z));
            if (checkPoint(Point3D(cur.x-1,cur.y,cur.z),cur) && toAdd.find(Point3D(cur.x-1,cur.y,cur.z)) == toAdd.cend()) toAdd.insert(Point3D(cur.x-1,cur.y,cur.z));
            if (checkPoint(Point3D(cur.x,cur.y+1,cur.z),cur) && toAdd.find(Point3D(cur.x,cur.y+1,cur.z)) == toAdd.cend()) toAdd.insert(Point3D(cur.x,cur.y+1,cur.z));
            if (checkPoint(Point3D(cur.x,cur.y-1,cur.z),cur) && toAdd.find(Point3D(cur.x,cur.y-1,cur.z)) == toAdd.cend()) toAdd.insert(Point3D(cur.x,cur.y-1,cur.z));
            if (checkPoint(Point3D(cur.x,cur.y,cur.z+1),cur) && toAdd.find(Point3D(cur.x,cur.y,cur.z+1)) == toAdd.cend()) toAdd.insert(Point3D(cur.x,cur.y,cur.z+1));
            if (checkPoint(Point3D(cur.x,cur.y,cur.z-1),cur) && toAdd.find(Point3D(cur.x,cur.y,cur.z-1)) == toAdd.cend()) toAdd.insert(Point3D(cur.x,cur.y,cur.z-1));
        }
    }

    void cleanPoints(set<Point3D>* s) {
        set<Point3D> toDelete = set<Point3D>();
        qDebug() << "before cleaning" << s->size();
        for (set<Point3D>::iterator it = s->begin();it!=s->end();it++){
            bool fx1 = 0, fx2 = 0, fy1 = 0, fy2 = 0;
            for (set<Point3D>::iterator it2 = s->begin();it2!=s->end();it2++){
                if (*it2==*it || it->z != it2->z) continue;
                if (it2->y == it->y && it2->x > it->x) fx1 = 1;
                if (it2->y == it->y && it2->x < it->x) fx2 = 1;
                if (it2->x == it->x && it2->y > it->y) fy1 = 1;
                if (it2->x == it->x && it2->y < it->y) fy2 = 1;
                if (fx1 && fx2 && fy1 && fy2) {
                    toDelete.insert(*it);
                    break;
                }
            }
        }
        for (set<Point3D>::iterator it = toDelete.begin();it!=toDelete.end();it++){
            s->erase(*it);
        }
        qDebug() << "after cleaning" << s->size();
    }

    void print(set<Point3D> s) {
        QString filename = "Data.txt";
        QFile file(filename);
        file.open(QIODevice::ReadWrite | QIODevice::Truncate);
        for (set<Point3D>::iterator it = s.begin();it!=s.end();it++){
            QTextStream stream(&file);
            stream << it->x << " " << it->y << " " << it->z << '\n';
        }
    }

    void connectionsLayer(Point3D first, set<Point3D>* p){
        if (p->find(first)!=p->cend()) return;
        p->insert(first);
        if (first.x<width-1 && points.find(Point3D(first.x+1,first.y,first.z))!=points.cend()) connectionsLayer(Point3D(first.x+1,first.y,first.z), p);
        if (first.x>0 && points.find(Point3D(first.x-1,first.y,first.z))!=points.cend()) connectionsLayer(Point3D(first.x-1,first.y,first.z), p);
        if (first.y<height-1 && points.find(Point3D(first.x,first.y+1,first.z))!=points.cend()) connectionsLayer(Point3D(first.x,first.y+1,first.z), p);
        if (first.y>0 && points.find(Point3D(first.x,first.y-1,first.z))!=points.cend()) connectionsLayer(Point3D(first.x,first.y-1,first.z), p);
    }

    void connections(Point3D first, set<Point3D>* p, int avoidLayer){
        if (p->find(first)!=p->cend()) return;
        p->insert(first);
        if (first.x<width-1 && points.find(Point3D(first.x+1,first.y,first.z))!=points.cend()) connections(Point3D(first.x+1,first.y,first.z), p, avoidLayer);
        if (first.x>0 && points.find(Point3D(first.x-1,first.y,first.z))!=points.cend()) connections(Point3D(first.x-1,first.y,first.z), p, avoidLayer);
        if (first.y<height-1 && points.find(Point3D(first.x,first.y+1,first.z))!=points.cend()) connections(Point3D(first.x,first.y+1,first.z), p, avoidLayer);
        if (first.y>0 && points.find(Point3D(first.x,first.y-1,first.z))!=points.cend()) connections(Point3D(first.x,first.y-1,first.z), p, avoidLayer);
        //if (first.z<imgQuan-1 && points.find(Point3D(first.x,first.y,first.z+1))!=points.cend() && first.z+1!=avoidLayer) connections(Point3D(first.x,first.y,first.z+1), p, avoidLayer);
        if (first.z>0 && points.find(Point3D(first.x,first.y,first.z-1))!=points.cend() && first.z-1!=avoidLayer) connections(Point3D(first.x,first.y,first.z-1), p, avoidLayer);
    }

    void findSeparation(){
        set<Point3D>* s = new set<Point3D>[imgQuan];
        for (int i=0;i<imgQuan;i++) s[i] = set<Point3D>();
        for (set<Point3D>::iterator it = points.begin();it!=points.end();it++){
            s[imgQuan - it->z - 1].insert(*it);
        }
        bool exitCycle = 0;
        int l;
        qDebug() << "looking for separation layer";
        for (int i=0;i<imgQuan;i++){
            qDebug() << "scanning layer" << imgQuan - i - 1;
            set<Point3D> p = set<Point3D>();
            connectionsLayer(*s[i].begin(),&p);
            while(p.size()!=s[i].size()){
                set<Point3D>::iterator it2 = p.begin();
                for (set<Point3D>::iterator it = s[i].begin();it!=s[i].end();it++){
                    if (*it!=*it2) {
                        int avoidLayer = it->z;
                        set<Point3D> s1 = set<Point3D>();
                        connections(*it,&s1,avoidLayer);
                        if (it2==p.cend()) it2--;
                        set<Point3D> s2 = set<Point3D>();
                        connections(*it2,&s2,avoidLayer);



                        if (s1.size() == 1) {
                            for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                if (I->z==avoidLayer) p.insert(*I);
                            break;
                        }
                        bool flagBreak = 0;
                        for (set<Point3D>::iterator it4 = s2.begin();it4!=s2.end() && !flagBreak;it4++){
                            //qDebug() << "s2" << it4->x << it4->y << it4->z;
                            for(set<Point3D>::iterator it3 = s1.begin();it3!=s1.end();it3++){
                                if (*it3==*it4) {
                                    for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                        if (I->z==avoidLayer) p.insert(*I);
                                    flagBreak = 1;
                                    break;
                                }
                            }
                        }
                        if (!flagBreak) {
                            bool f1 = 0, f2 = 0;
                            for (set<Point3D>::iterator it4 = s2.begin();it4!=s2.end();it4++){
                                if (it4->z==0) {
                                    f1 = 1;
                                    break;
                                }
                            }
                            for (set<Point3D>::iterator it4 = s1.begin();it4!=s1.end();it4++){
                                if (it4->z==0) {
                                    f2 = 1;
                                    break;
                                }
                            }
                            if (f1==0) {
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                    if (I->z==avoidLayer) {
                                        p.insert(*I);
                                    }
                            }
                            if (f2==0) {
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                    if (I->z==avoidLayer) {
                                        p.insert(*I);
                                    }
                            }
                            if (f1 && f2) {
                                exitCycle = 1;
                                l=avoidLayer;
                                separationLayer = avoidLayer;
                                for (set<Point3D>::iterator I = s2.begin();I!=s2.end();I++) pointsSep2.insert(*I);
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++) pointsSep1.insert(*I);
                                for (int j = 0;j<imgQuan - avoidLayer - 1;j++){
                                    for (set<Point3D>::iterator I = s[j].begin();I!=s[j].end();I++) pointsCom.insert(*I);
                                }
                            }
                        }
                        break;
                    }
                    it2++;
                }
                if(exitCycle) break;
            }
            if(exitCycle) break;
        }
        if (!exitCycle) {
            qDebug() << "separation layer not found";
            delete[] s;
            return;
        }
        qDebug() << "found separation on" << l << "layer";
        delete[] s;
    }

    set<Point3D> getCenters(set<Point3D> s){
        set<Point3D> centers = set<Point3D>();
        int firstLayerNum = s.begin()->z, lastLayerNum = (--s.end())->z;
        int layersNum = lastLayerNum - firstLayerNum + 1;
        double* summsX = new double[layersNum], * summsY = new double[layersNum], * col = new double[layersNum];
        for (int i=0;i<layersNum;i++){
            summsX[i] = 0;
            summsY[i] = 0;
            col[i] = 0;
        }
        for (set<Point3D>::iterator it = s.begin();it!=s.end();it++){
            summsX[it->z - firstLayerNum] += it->x;
            summsY[it->z - firstLayerNum] += it->y;
            col[it->z - firstLayerNum]++;
        }
        for (int i=0;i<layersNum;i++){
            centers.insert(Point3D(summsX[i]/col[i],summsY[i]/col[i],i + firstLayerNum));
        }
        delete[] summsX;
        delete[] summsY;
        delete[] col;

        return centers;
    }

    void setAllCenters() {
        centersCom = getCenters(pointsCom);
        centersSep1 = getCenters(pointsSep1);
        centersSep2 = getCenters(pointsSep2);
    }

    void setRads(set<Point3D> p, set<Point3D> c, vector<double> *v) {
        set<Point3D>::iterator it1 = c.begin();
        set<Point3D>::iterator it2 = c.begin();
        it2++;
        for(int i=0;i<c.size() - 1;i++){
            double* otr = new double[3];
            otr[0] = it2->x-it1->x;
            otr[1] = it2->y-it1->y;
            otr[2] = (it2->z-it1->z)*thickness;
            Point3D center = Point3D(it1->x+otr[0]/2,it1->y+otr[1]/2,it1->z*thickness+otr[2]/2);
            double centerX = it1->x+otr[0]/2;
            double centerY = it1->y+otr[1]/2;
            double centerZ = it1->z*thickness+otr[2]/2;
            double A = otr[0];
            double B = otr[1];
            double C = otr[2];
            double D = -otr[0]*centerX-otr[1]*centerY-otr[2]*centerZ;
            it1++; it2++;
            double s = 0;
            int l = 0;
            for (set<Point3D>::iterator it = p.begin();it!=p.end();it++){
                if(fabs(A*it->x+B*it->y+C*it->z*thickness+D)/sqrt(A*A+B*B+C*C) <= 0.5*thickness) {
                    s+=sqrt(pow(it->x-centerX,2)+pow(it->y-centerY,2)+pow(it->z*thickness-centerZ,2));
                    l++;
                }
            }
            delete[] otr;
            v->push_back(s/l);
        }

        double prev = v->at(0);
        for (int i=1;i<v->size();i++){
            if (1.5*prev<v->at(i)) v->at(i) = prev;
            prev = v->at(i);
        }
        prev = v->at(v->size()-1);
        for (int i=v->size()-2;i>=0;i--){
            if (1.5*prev<v->at(i)) v->at(i) = prev;
            prev = v->at(i);
        }
    }

    void setAllRads() {
        setRads(pointsCom, centersCom, &radsCom);
        setRads(pointsSep1, centersSep1, &radsSep1);
        setRads(pointsSep2, centersSep2, &radsSep2);
    }


    void cleanWrongSeparation(set<Point3D>* po, int layersNum){
        set<Point3D> toDelete = set<Point3D>();
        set<Point3D>* s = new set<Point3D>[layersNum];
        for (int i=0;i<layersNum;i++) s[i] = set<Point3D>();
        for (set<Point3D>::iterator it = po->begin();it!=po->end();it++){
            s[layersNum - it->z - 1].insert(*it);
        }
        bool exitCycle = 0;
        qDebug() << "looking for wrong separation layer";
        for (int i=0;i<layersNum;i++){
            qDebug() << "scanning layer" << layersNum - i - 1;
            set<Point3D> p = set<Point3D>();
            connectionsLayer(*s[i].begin(),&p);
            int oldSize = p.size();
            while(p.size()!=s[i].size()){
                set<Point3D>::iterator it2 = p.begin();
                bool f = 0;
                for (set<Point3D>::iterator it = s[i].begin();it!=s[i].end();it++){
                    if (*it!=*it2 && toDelete.find(*it) == toDelete.cend() && toDelete.find(*it2) == toDelete.cend()) {
                        int avoidLayer = it->z;
                        set<Point3D> s1 = set<Point3D>();
                        connections(*it,&s1,avoidLayer);
                        if (it2==p.cend()) it2--;
                        set<Point3D> s2 = set<Point3D>();
                        connections(*it2,&s2,avoidLayer);



                        if (s1.size() == 1) {
                            for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                if (I->z==avoidLayer) p.insert(*I);
                            break;
                        }
                        bool flagBreak = 0;
                        for (set<Point3D>::iterator it4 = s2.begin();it4!=s2.end() && !flagBreak;it4++){
                            for(set<Point3D>::iterator it3 = s1.begin();it3!=s1.end();it3++){
                                if (*it3==*it4) {
                                    for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                        if (I->z==avoidLayer) p.insert(*I);
                                    flagBreak = 1;
                                    break;
                                }
                            }
                        }
                        if (s1.size()==s2.size()) break;
                        if (!flagBreak) {
                            bool f1 = 0, f2 = 0;
                            for (set<Point3D>::iterator it4 = s2.begin();it4!=s2.end();it4++){
                                if (it4->z==0) {
                                    f1 = 1;
                                    break;
                                }
                            }
                            for (set<Point3D>::iterator it4 = s1.begin();it4!=s1.end();it4++){
                                if (it4->z==0) {
                                    f2 = 1;
                                    break;
                                }
                            }
                            if (f1==0) {
                                for (set<Point3D>::iterator I = s2.begin();I!=s2.end();I++)
                                    toDelete.insert(*I);
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                    if (I->z==avoidLayer) p.insert(*I);
                            }
                            if (f2==0) {
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                    toDelete.insert(*I);
                                for (set<Point3D>::iterator I = s1.begin();I!=s1.end();I++)
                                    if (I->z==avoidLayer) p.insert(*I);
                            }
                        }
                        f = 1;
                        break;
                    }
                    it2++;
                }
                if (p.size() == oldSize) break;
                oldSize = p.size();
                if (!f) break;
                if(exitCycle) break;
            }
            if(exitCycle) break;
        }
        delete[] s;
        for (set<Point3D>::iterator it = toDelete.begin();it!=toDelete.end();it++){
            po->erase(*it);
        }
    }

    void init() {
        setPoints();
        findSeparation();
        cleanWrongSeparation(&(this->pointsSep1), separationLayer + 1);
        cleanWrongSeparation(&(this->pointsSep2), separationLayer + 1);
        cleanPoints(&(this->pointsCom));
        cleanPoints(&(this->pointsSep1));
        cleanPoints(&(this->pointsSep2));
        setAllCenters();
        setAllRads();
    }

    void findStenos() {
        vector<double> rads;
        set<Point3D>::iterator p2 = centersSep1.end();
        set<Point3D>::iterator p3 = centersSep2.end();
        p2--; p3--;
        if (p3->x < p2->x) rads = radsSep1;
        else {
            rads = radsSep2;
            set<Point3D> temp = pointsSep1;
            pointsSep1 = pointsSep2;
            pointsSep2 = temp;
        }
        double min = rads[rads.size()-1];
        int minL = rads.size()-1;
        for (int i=rads.size()-2;i>=rads.size()/2;i--){
            if (rads[i]<min) {
                min = rads[i];
                minL = i;
            }
        }
        double max = rads[minL];
        int maxL = minL;
        for (int i=minL;i>=0;i--)
            if (rads[i]>max){
                max = rads[i];
                maxL = i;
            }
        stenos = (max - min) / max;
        stenosLayers[0] = minL;
        stenosLayers[1] = maxL;
        stenosMinPoints.clear();
        stenosMaxPoints.clear();

        set<Point3D> c = centersSep1;
        set<Point3D> p = pointsSep1;
        set<Point3D>::iterator it1 = c.begin();
        set<Point3D>::iterator it2 = c.begin();
        it2++;
        for(int i=0;i<c.size() - 1;i++){
            if (it1->z != minL && it1->z != maxL) {
                it1++; it2++; continue;
            }
            double* otr = new double[3];
            otr[0] = it2->x-it1->x;
            otr[1] = it2->y-it1->y;
            otr[2] = (it2->z-it1->z)*thickness;
            double centerX = it1->x+otr[0]/2;
            double centerY = it1->y+otr[1]/2;
            double centerZ = it1->z*thickness+otr[2]/2;
            double A = otr[0];
            double B = otr[1];
            double C = otr[2];
            double D = -otr[0]*centerX-otr[1]*centerY-otr[2]*centerZ;
            for (set<Point3D>::iterator it = p.begin();it!=p.end();it++){
                if(fabs(A*it->x+B*it->y+C*it->z*thickness+D)/sqrt(A*A+B*B+C*C) <= 0.5*thickness) {
                    if (it1->z == minL) stenosMinPoints.insert(*it);
                    if (it1->z == maxL) stenosMaxPoints.insert(*it);
                }
            }
            it1++; it2++;
        }
    }
};

struct nums {
    int *i,*j;
    DicomData*** dataset;
    cv::Mat** dst;
    DicomImage*** image;
    nums(int* i,int* j, DicomData*** dataset, cv::Mat** dst, DicomImage*** image){
        this->i=i;
        this->j=j;
        this->dataset = dataset;
        this->dst = dst;
        this->image = image;
    }
};

void getScreenResolution (int &width, int &height) {
    Display* disp = XOpenDisplay(NULL);
    Screen* scrn = DefaultScreenOfDisplay(disp);
    width = scrn->width;
    height = scrn->height;
    XCloseDisplay(disp);
}

Mat renderText(Mat dst, DicomData** dataset, int i);

int wWidth, wHeight;
int oldx = INFINITY, oldy = INFINITY;
bool changingWindow = false;
void mouseCallBack(int event, int x, int y, int flag, void* userdata) {
    nums* n = (nums*)userdata;
    int nWidth = n->dataset[*(n->j)][0]->width;
    int nHeight = n->dataset[*(n->j)][0]->height;
    double scale = min((double) wWidth/nWidth,(double) wHeight/nHeight)/R;
    if (event == cv::EVENT_LBUTTONDOWN) {
        if (*(n->i)!=0 && *(n->i)!=n->dataset[*(n->j)][0]->imgQuan-1) {
            qDebug() << "Cant set start at middle layer";
            return;
        }
        qDebug() << "Right start set at" << int(x/scale) << int(y/scale) << *(n->i) << n->dataset[*(n->j)][0]->getValue(Point3D(int(y/scale), int(x/scale), *(n->i)));
        n->dataset[*(n->j)][0]->setStart(int(y/scale),int(x/scale),*(n->i));
    }
    else if (event == cv::EVENT_RBUTTONDOWN) {
        if (*(n->i)!=0 && *(n->i)!=n->dataset[*(n->j)][0]->imgQuan-1) {
            qDebug() << "Cant set start at middle layer";
            return;
        }
        qDebug() << "Left start set at" << int(x/scale) << int(y/scale) << *(n->i) << n->dataset[*(n->j)][1]->getValue(Point3D(int(y/scale), int(x/scale), *(n->i)));
        n->dataset[*(n->j)][1]->setStart(int(y/scale),int(x/scale),*(n->i));
    }
    if (event == cv::EVENT_MOUSEMOVE && oldx==INFINITY) {
        oldx = x;
        oldy = y;
    }
    int dx = x - oldx;
    int dy = y - oldy;

    if (event == cv::EVENT_MBUTTONDOWN){
        changingWindow = true;
    }

    if (changingWindow && event == cv::EVENT_MOUSEMOVE) {
        double c,w;
        n->image[*(n->j)][0]->getWindow(c,w);
        c+=dx;
        w+=dy;
        for (int I = 0;I < n->dataset[*(n->j)][0]->imgQuan;I++) {
            n->image[*(n->j)][I]->setWindow(c,w);
            Sint16* pixelData = (Sint16*)(n->image[*(n->j)][I]->getOutputData(16));
            if (pixelData != NULL)
            {
                n->dst[*(n->j)][I] = cv::Mat(nHeight, nWidth, CV_16SC1, pixelData);
                cv::Mat res;
                cv::resize(n->dst[*(n->j)][I],res,cv::Size(nHeight*scale,nWidth*scale),0,0,cv::INTER_LINEAR_EXACT);
                n->dst[*(n->j)][I] = res;
            }
        }
        n->image[*(n->j)][0]->getWindow(c,w);
        n->dataset[*(n->j)][0]->MAX_VAL = c+w/2;
        n->dataset[*(n->j)][0]->MIN_VAL = c-w/2;
        n->dataset[*(n->j)][1]->MAX_VAL = c+w/2;
        n->dataset[*(n->j)][1]->MIN_VAL = c-w/2;
        imshow(windowName, n->dst[*(n->j)][*(n->i)]);
    }

    oldx = x;
    oldy = y;
    if (event == cv::EVENT_MBUTTONUP){
        changingWindow = false;
    }

    Mat rendered = renderText(n->dst[*(n->j)][*(n->i)],n->dataset[*(n->j)],*(n->i));
    imshow(windowName,rendered);
}

void plot_points(set<Point3D> points, double thicknessZ, vector<int> surfaces = vector<int>()) {
    vector<int> x = vector<int>();
    vector<int> y = vector<int>();
    vector<double> z = vector<double>();
    int xmin = INFINITY, ymin = INFINITY, xmax = 0, ymax = 0;
    for (set<Point3D>::iterator it = points.begin();it!=points.end();it++){
        x.push_back(it->x);
        y.push_back(it->y);
        z.push_back(it->z*thicknessZ);
        if (it->x < xmin) xmin = it->x;
        if (it->x > xmax) xmax = it->x;
        if (it->y < ymin) ymin = it->y;
        if (it->y > ymax) ymax = it->y;

    }
    Gnuplot g1;
    g1.unset_grid();

    if (surfaces.size()!=0){
        vector<double> x1 = vector<double>();
        vector<double> y1 = vector<double>();
        vector<double> z1 = vector<double>();
        g1.set_multiplot();
        g1.plot_xyz(x,y,z);
        g1.set_style("dots");
        for (int k = 0 ;k < surfaces.size();k++) {
            for (int j = ymin;j<ymax;j+=1){
                for (int i = xmin;i<xmax;i+=1) {
                    x1.push_back(i);
                    y1.push_back(j);
                    z1.push_back(surfaces[k]*thicknessZ);
                }
            }
            g1.plot_xyz(x1,y1,z1);
            x1.clear();
            y1.clear();
            z1.clear();
        }
        g1.unset_multiplot();
    }
    else {
        g1.plot_xyz(x,y,z);
    }

    waitKey();


}

Mat renderText(Mat dst, DicomData** dataset, int i) {
    Mat dstRender = dst.clone();
    double lineHeight = 30;
    double fontScale = 2.0;
    cv::putText(dstRender,"WL/WW: " + to_string((dataset[0]->MAX_VAL + dataset[0]->MIN_VAL)/2) + "/" + to_string(dataset[0]->MAX_VAL - dataset[0]->MIN_VAL),Point(0,dstRender.rows),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"MAX CONTRAST: " + to_string(dataset[0]->MAX_CONTRAST),Point(0,dstRender.rows - lineHeight*1),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Right stenos: " + to_string((int)(dataset[0]->stenos * 100)) + "%",Point(0,dstRender.rows - lineHeight*2),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Left stenos: " + to_string((int)(dataset[1]->stenos * 100)) + "%",Point(0,dstRender.rows - lineHeight*3),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Current layer: " + to_string(i) + "/" + to_string(dataset[0]->imgQuan-1),Point(0,dstRender.rows - lineHeight*4),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Right separation layer: " + to_string(dataset[0]->separationLayer),Point(0,dstRender.rows - lineHeight*5),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Left separation layer: " + to_string((int)dataset[1]->separationLayer),Point(0,dstRender.rows - lineHeight*6),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Right start set at: " + to_string(dataset[0]->start.y) + " " + to_string(dataset[0]->start.x)+ " " + to_string(dataset[0]->start.z),Point(0,dstRender.rows - lineHeight*7),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    cv::putText(dstRender,"Left start set at: " + to_string(dataset[1]->start.y) + " " + to_string(dataset[1]->start.x)+ " " + to_string(dataset[1]->start.z),Point(0,dstRender.rows - lineHeight*8),FONT_HERSHEY_PLAIN, fontScale,Scalar(10000000),3);
    double lineHeightSmall = 15;
    double fontScaleSmall = 1.0;
    cv::putText(dstRender,"a/s to change patient",Point(dstRender.cols - 300,dstRender.rows - lineHeightSmall * 4),FONT_HERSHEY_PLAIN, fontScaleSmall,Scalar(10000000),1);
    cv::putText(dstRender,"z/x to change layer",Point(dstRender.cols - 300,dstRender.rows - lineHeightSmall * 3),FONT_HERSHEY_PLAIN, fontScaleSmall,Scalar(10000000),1);
    cv::putText(dstRender,"RMB/LMB to set right/left start",Point(dstRender.cols - 300,dstRender.rows - lineHeightSmall * 2),FONT_HERSHEY_PLAIN, fontScaleSmall,Scalar(10000000),1);
    cv::putText(dstRender,"p/l to plot right/left",Point(dstRender.cols - 300,dstRender.rows - lineHeightSmall * 1),FONT_HERSHEY_PLAIN, fontScaleSmall,Scalar(10000000),1);
    cv::putText(dstRender,"o/k to find stenos on right/left",Point(dstRender.cols - 300,dstRender.rows - lineHeightSmall * 0),FONT_HERSHEY_PLAIN, fontScaleSmall,Scalar(10000000),1);
    return dstRender;
}


int main(int argc, char** argv)
{
    setlocale(LC_ALL, "Russian");
    getScreenResolution(wWidth,wHeight);

    vector<vector<string>> files;
    vector<string> subdirectories;
    int i = 0;
    int j = 0;
    stdfs::path path = "../marked";




    for (auto &iter : stdfs::directory_iterator(path)){
        subdirectories.push_back(string(iter.path().string()));
        files.push_back(vector<string>());
        for (auto &iter2 : stdfs::directory_iterator(iter.path())){
            files.back().push_back(string(iter2.path().string()));
        }
        sort(files.back().begin(),files.back().end());
    }


    cv::Mat** dst = new Mat*[files.size()];
    DcmFileFormat** image_file = new DcmFileFormat*[files.size()];
    DicomImage*** image = new DicomImage**[files.size()];
    OFString* names = new OFString[files.size()];
    double* thicknesses = new double[files.size()];
    for (int I = 0;I<files.size();I++){
        dst[I] = new Mat[files[I].size()];
        image_file[I] = new DcmFileFormat[files[I].size()];
        image[I] = new DicomImage*[files[I].size()];
        for(int J=0;J<files[I].size();J++){
            image_file[I][J] = DcmFileFormat();
            QString name = QString::fromStdString(files[I][J]);
            OFCondition status = image_file[I][J].loadFile(name.toStdString().c_str());
            image[I][J] = new DicomImage(name.toStdString().c_str());
            int nWidth = image[I][J]->getWidth();
            int nHeight = image[I][J]->getHeight();
            double scale = min((double) wWidth/nWidth,(double) wHeight/nHeight)/R;
            image[I][J]->setWindow(100,400);
            OFString patientName;
            if (image_file[I][J].getDataset()->findAndGetOFString(DCM_PatientName, patientName).good())
            {
                names[I] = patientName;
            } else
                names[I] = "NONAME";
            if (image[I][J] != NULL)
            {
                if (image[I][J]->getStatus() == EIS_Normal)
                {
                    Sint16* pixelData = (Sint16*)(image[I][J]->getOutputData(16));
                    if (pixelData != NULL)
                    {
                        dst[I][J] = cv::Mat(nHeight, nWidth, CV_16SC1, pixelData);
                        cv::Mat res;
                        cv::resize(dst[I][J],res,cv::Size(nHeight*scale,nWidth*scale),0,0,cv::INTER_LINEAR_EXACT);
                        dst[I][J] = res;
                    }
                }
                else
                    qDebug() << "Error: cannot load DICOM image (" << DicomImage::getString(image[I][J]->getStatus()) << ")";
            }
        }
        OFString thicknessStr;
        image_file[I][0].getDataset()->findAndGetOFString(DcmTagKey(0x0018,0x0050), thicknessStr);
        thicknesses[I] = stod(thicknessStr.c_str());
    }

    DicomData*** dataset = new DicomData**[files.size()];
    for (int I = 0;I<files.size();I++){
        dataset[I] = new DicomData*[2];
        dataset[I][0] = new DicomData(image[I],files[I].size());
        dataset[I][0]->thickness = thicknesses[I];
        dataset[I][0]->name = names[I].c_str();
        dataset[I][1] = new DicomData(image[I],files[I].size());
        dataset[I][1]->thickness = thicknesses[I];
        dataset[I][1]->name = names[I].c_str();
    }

    string name = windowName;
    cv::namedWindow(name,1);
    nums n = nums(&i,&j, dataset, dst, image);
    cv::setMouseCallback(name, mouseCallBack, &n);
    setWindowTitle(name,("Patient: " + names[j]).c_str());
    while (true)
    {
        Mat dstRender = renderText(dst[j][i],dataset[j],i);
        cv::imshow(name, dstRender);
        int k = cv::waitKey();
        if (k == 120) { //z
            if (i!=0) i--;
        }
        if (k == 122) { //x
            if (i!=files[j].size()-1) i++;
        }
        if (k == 97) { //a
            if (j!=0) {
                j--;
                i=0;
                setWindowTitle(name,("Patient: " + names[j]).c_str());
            }
        }
        if (k == 115) { //s
            if (j!=files.size()-1){
                j++;
                i=0;
                setWindowTitle(name,("Patient: " + names[j]).c_str());
            }
        }
        if (k == 112) { //p
            //qDebug() << dataset[j][0]->MIN_VAL << dataset[j][0]->MAX_VAL;
            dataset[j][0]->PixelData = dataset[j][0]->getPixelData(image[j],dataset[j][0]->imgQuan);
            dataset[j][0]->reset();
            dataset[j][0]->setPoints();
            plot_points(dataset[j][0]->points, dataset[j][0]->thickness);
        }
        if (k == 108) { //l
            dataset[j][1]->PixelData = dataset[j][1]->getPixelData(image[j],dataset[j][1]->imgQuan);
            dataset[j][1]->reset();
            dataset[j][1]->setPoints();
            plot_points(dataset[j][1]->points, dataset[j][1]->thickness);
        }
        if (k == 171) { //+
            dataset[j][0]->MAX_CONTRAST+=1;
            dataset[j][1]->MAX_CONTRAST+=1;
        }
        if (k == 173) { //-
            if (dataset[j][0]->MAX_CONTRAST > 0) {
                dataset[j][0]->MAX_CONTRAST-=1;
                dataset[j][1]->MAX_CONTRAST-=1;
            }
        }
        if (k == 111) { //o
            dataset[j][0]->reset();
            dataset[j][0]->init();
            dataset[j][0]->findStenos();
            qDebug() << "Stenos percent:" <<dataset[j][0]->stenos;
            Gnuplot g1("points");
            vector<double> x = vector<double>();
            vector<double> y = vector<double>();
            vector<double> z = vector<double>();
            int xmin = INFINITY, ymin = INFINITY, xmax = 0, ymax = 0;
            for (set<Point3D>::iterator it = dataset[j][0]->points.begin();it!=dataset[j][0]->points.end();it++){
                if (dataset[j][0]->stenosMinPoints.find(*it) == dataset[j][0]->stenosMinPoints.cend() && dataset[j][0]->stenosMaxPoints.find(*it) == dataset[j][0]->stenosMaxPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][0]->thickness);
                }

                if (it->x < xmin) xmin = it->x;
                if (it->x > xmax) xmax = it->x;
                if (it->y < ymin) ymin = it->y;
                if (it->y > ymax) ymax = it->y;

            }
            g1.set_multiplot();
            g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();

            for (set<Point3D>::iterator it = dataset[j][0]->pointsSep1.begin();it!=dataset[j][0]->pointsSep1.end();it++){
                if (dataset[j][0]->stenosMinPoints.find(*it) != dataset[j][0]->stenosMinPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][0]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            for (set<Point3D>::iterator it = dataset[j][0]->pointsSep1.begin();it!=dataset[j][0]->pointsSep1.end();it++){
                if (dataset[j][0]->stenosMaxPoints.find(*it) != dataset[j][0]->stenosMaxPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][0]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            for (int I = xmin; I < xmax ; I+=2){
                for (int J = ymin; J < ymax ; J+=2) {
                    x.push_back(I);
                    y.push_back(J);
                    z.push_back(dataset[j][0]->separationLayer*dataset[j][0]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            g1.unset_multiplot();
            waitKey();
        }
        if (k == 107) { //k
            dataset[j][1]->reset();
            dataset[j][1]->init();
            dataset[j][1]->findStenos();
            qDebug() << "Stenos percent:" <<dataset[j][1]->stenos;
            Gnuplot g1("points");
            vector<double> x = vector<double>();
            vector<double> y = vector<double>();
            vector<double> z = vector<double>();
            int xmin = INFINITY, ymin = INFINITY, xmax = 0, ymax = 0;
            for (set<Point3D>::iterator it = dataset[j][1]->points.begin();it!=dataset[j][1]->points.end();it++){
                if (dataset[j][1]->stenosMinPoints.find(*it) == dataset[j][1]->stenosMinPoints.cend() && dataset[j][1]->stenosMaxPoints.find(*it) == dataset[j][1]->stenosMaxPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][1]->thickness);
                }

                if (it->x < xmin) xmin = it->x;
                if (it->x > xmax) xmax = it->x;
                if (it->y < ymin) ymin = it->y;
                if (it->y > ymax) ymax = it->y;

            }
            g1.set_multiplot();
            g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();

            for (set<Point3D>::iterator it = dataset[j][1]->pointsSep1.begin();it!=dataset[j][1]->pointsSep1.end();it++){
                if (dataset[j][1]->stenosMinPoints.find(*it) != dataset[j][1]->stenosMinPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][1]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            for (set<Point3D>::iterator it = dataset[j][1]->pointsSep1.begin();it!=dataset[j][1]->pointsSep1.end();it++){
                if (dataset[j][1]->stenosMaxPoints.find(*it) != dataset[j][1]->stenosMaxPoints.cend()) {
                    x.push_back(it->x);
                    y.push_back(it->y);
                    z.push_back(it->z*dataset[j][1]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            for (int I = xmin; I < xmax ; I+=2){
                for (int J = ymin; J < ymax ; J+=2) {
                    x.push_back(I);
                    y.push_back(J);
                    z.push_back(dataset[j][0]->separationLayer*dataset[j][0]->thickness);
                }
            }
            if (x.size()!=0) g1.plot_xyz(x,y,z);
            x.clear(); y.clear(); z.clear();
            g1.unset_multiplot();
            waitKey();
        }
        if (k == 27 || k == -1) //Esc
            break;
    }

    return 0;
}
