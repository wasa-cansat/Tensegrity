#include "Util.h"

Vec3 vec3(float x, float y, float z) {
    Quaternion v(x, y, z);
    return v;
}

Vec3 zero() {
    return vec3(0, 0, 0);
}


void printQuaternion(Quaternion q, Print &S) {
    S.print(",p:"); S.println(q.a, 4);  //q0が特別な奴
    S.print(",q:"); S.println(q.b, 4);
    S.print(",r:"); S.println(q.c, 4);
    S.print(",s:"); S.println(q.d, 4);
}

void printVec3(Vec3 v, char prefix, Print &S) {
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("x:"); S.println(v.b, 4);
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("y:"); S.println(v.c, 4);
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("z:"); S.println(v.d, 4);
}

