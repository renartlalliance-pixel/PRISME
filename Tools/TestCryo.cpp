// Test CRYSTALLIZE : bypass exact, shelf, saturation, largeur, stabilite, niveau.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/Crystallize.h"

static bool finite(float x) { return std::isfinite(x); }

static double binMag(const std::vector<float>& v, double f, double sr)
{
    // DFT a une frequence (Goertzel simplifie sur tout le buffer)
    double re = 0, im = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        double ph = 6.283185307179586 * f * (double) i / sr;
        re += v[i] * std::cos(ph);
        im += v[i] * std::sin(ph);
    }
    return std::sqrt(re * re + im * im) / v.size();
}

static double rms(const std::vector<float>& v)
{
    double e = 0;
    for (auto x : v) e += x * x;
    return std::sqrt(e / v.size());
}

static void run(Crystallize& c, const std::vector<float>& in,
                std::vector<float>& L, std::vector<float>& R, float m)
{
    L.assign(in.size(), 0); R.assign(in.size(), 0);
    for (size_t i = 0; i < in.size(); ++i)
    {
        float a = 0, b = 0;
        c.process(in[i], in[i], a, b, m);
        L[i] = a; R[i] = b;
    }
}

int main()
{
    const double sr = 44100.0;
    const int n = (int)(sr * 2);
    bool ok = true;
    auto check = [&](const char* name, bool cond, const char* detail = "")
    {
        printf("%-28s %s %s\n", name, cond ? "OK" : "PROBLEME", detail);
        if (! cond) ok = false;
    };

    Crystallize c;
    c.setSampleRate(sr);
    std::vector<float> L, R;

    // 1. m=0 : bypass bit-exact
    {
        c.reset();
        std::vector<float> in(n);
        srand(11);
        for (auto& v : in) v = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
        run(c, in, L, R, 0.0f);
        double d = 0;
        for (size_t i = 0; i < in.size(); ++i) d = std::max(d, (double) std::fabs(L[i] - in[i]));
        check("1. bypass 0% exact", d == 0.0);
    }

    // 2. Shelf : aigus nettement releves a m=100 % vs m=15 % (meme bruit, bande 9-11 kHz)
    {
        auto bandEnergy = [&](float m)
        {
            c.reset();
            srand(21);
            std::vector<float> in(n);
            for (auto& v : in) v = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
            run(c, in, L, R, m);
            double e = 0;
            for (double f = 9000; f <= 11000; f += 100)
            {
                double b = binMag(L, f, sr);
                e += b * b;
            }
            return std::sqrt(e);
        };
        double e015 = bandEnergy(0.15f), e10 = bandEnergy(1.0f);
        char buf[64]; snprintf(buf, sizeof buf, "(15%%=%.4f 100%%=%.4f x%.1f)", e015, e10, e10 / (e015 + 1e-12));
        // Seuil x2 (+6 dB) : audible sans exces, conforme spec (pas d'amplification excessive)
        check("2. aigus releves", e10 > e015 * 2.0, buf);
    }

    // 3. Saturation : harmoniques 2f+3f plus fortes a 100 % qu'a 20 %
    {
        auto thd = [&](float m)
        {
            c.reset();
            std::vector<float> in(n);
            for (size_t i = 0; i < in.size(); ++i)
                in[i] = 0.4f * (float) std::sin(6.2831853 * 1000.0 * i / sr);
            run(c, in, L, R, m);
            double f1 = binMag(L, 1000, sr);
            return (binMag(L, 2000, sr) + binMag(L, 3000, sr)) / (f1 + 1e-12);
        };
        double t02 = thd(0.2f), t10 = thd(1.0f);
        char buf[64]; snprintf(buf, sizeof buf, "(20%%=%.4f 100%%=%.4f)", t02, t10);
        check("3. harmoniques croissantes", t10 > t02 * 3.0, buf);
    }

    // 4. Largeur : L != R sur entree mono + compat mono
    {
        c.reset();
        std::vector<float> in(n, 0.0f);
        for (size_t i = 0; i < in.size(); ++i)
            in[i] = 0.3f * (float) std::sin(6.2831853 * 440.0 * i / sr);
        run(c, in, L, R, 1.0f);
        double d = 0;
        for (size_t i = 0; i < in.size(); ++i) d += std::fabs(L[i] - R[i]);
        d /= in.size();
        std::vector<float> mid(in.size());
        for (size_t i = 0; i < in.size(); ++i) mid[i] = (L[i] + R[i]) * 0.5f;
        double compat = rms(mid) / (std::sqrt((rms(L) * rms(L) + rms(R) * rms(R)) / 2.0) + 1e-12);
        char buf[64]; snprintf(buf, sizeof buf, "(largeur=%.4f compat=%.2f)", d, compat);
        check("4. stereo + mono OK", d > 0.001 && compat > 0.7, buf);
    }

    // 5+6. Stabilite 44.1/48/96 kHz + niveau coherent (bruit blanc)
    {
        bool allOk = true;
        for (double s2 : { 44100.0, 48000.0, 96000.0 })
        {
            Crystallize c2;
            c2.setSampleRate(s2);
            c2.reset();
            int n2 = (int)(s2 * 2);
            srand(5);
            double e0 = 0, e1 = 0, peak = 0;
            for (int i = 0; i < n2; ++i)
            {
                float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
                float a, b;
                c2.process(nz, nz, a, b, 1.0f);
                if (! finite(a) || ! finite(b)) { allOk = false; break; }
                e0 += nz * nz; e1 += a * a;
                peak = std::max<double>(peak, std::fabs(a));
            }
            double ratio = std::sqrt(e1 / e0);
            char buf[96];
            snprintf(buf, sizeof buf, "(%.0f Hz : ratio=%.2f peak=%.2f)", s2, ratio, peak);
            bool thisOk = allOk && ratio < 3.0 && peak < 1.0;
            printf("%-28s %s %s\n", "5/6. stable + niveau", thisOk ? "OK" : "PROBLEME", buf);
            if (! thisOk) ok = false;
        }
    }

    printf(ok ? "CRYSTALLIZE OK\n" : "CRYSTALLIZE PROBLEME\n");
    return ok ? 0 : 1;
}
