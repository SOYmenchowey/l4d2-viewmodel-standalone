#pragma once
#include "imgui.h"
#include <cstdint>
#include <cstring>

// Owned by the menu/Present thread. No game access, windows, input or workers.
namespace notices {
    struct Entry {
        char name[80]{};
        bool enabled=false;
        std::uint64_t started=0;
    };
    struct Queue {
        static constexpr int Capacity=4;
        static constexpr std::uint64_t Duration=2400, Fade=400;
        Entry entries[Capacity]{};
        int count=0;

        void Clear() { count=0; }
        void Expire(std::uint64_t now) {
            int remaining=0;
            for(int i=0;i<count;++i)
                if(now>=entries[i].started && now-entries[i].started<Duration)
                    entries[remaining++]=entries[i];
            count=remaining;
        }
        void Show(const char* name,bool enabled,std::uint64_t now) {
            if(!name || !*name) return;
            Expire(now);
            Entry entry{};
            strncpy_s(entry.name,name,_TRUNCATE);
            entry.enabled=enabled; entry.started=now;
            // Rapid ON/OFF replaces the old notice, leaving only the current state.
            for(int i=0;i<count;++i) if(!strcmp(entries[i].name,entry.name)) {
                for(int j=i;j<count-1;++j) entries[j]=entries[j+1];
                --count; break;
            }
            if(count<Capacity) ++count;
            for(int i=count-1;i>0;--i) entries[i]=entries[i-1];
            entries[0]=entry;
        }
        void Draw(std::uint64_t now) {
            Expire(now);
            if(!count || !ImGui::GetCurrentContext()) return;
            const ImVec2 size=ImGui::GetIO().DisplaySize;
            const float margin=12, height=56, gap=8;
            const float width=ImMin(292.f,size.x-2*margin);
            if(width<100 || size.y<height+2*margin) return;
            auto* dl=ImGui::GetForegroundDrawList();
            const float x=size.x-margin-width;
            dl->PushClipRect(ImVec2(0,0),size,true);
            for(int i=0;i<count;++i) {
                const float y=margin+i*(height+gap);
                if(y+height>size.y-margin) break;
                const auto& entry=entries[i];
                const auto age=now-entry.started;
                const float alpha=age>Duration-Fade ? float(Duration-age)/float(Fade) : 1.f;
                const ImU32 accent=entry.enabled ? IM_COL32(140,219,184,int(255*alpha)) : IM_COL32(240,138,130,int(255*alpha));
                dl->AddRectFilled(ImVec2(x,y),ImVec2(x+width,y+height),IM_COL32(17,18,20,int(238*alpha)),7);
                dl->AddRect(ImVec2(x,y),ImVec2(x+width,y+height),IM_COL32(55,59,63,int(220*alpha)),7);
                dl->AddRectFilled(ImVec2(x,y+10),ImVec2(x+3,y+height-10),accent,2);
                dl->PushClipRect(ImVec2(x+12,y),ImVec2(x+width-12,y+height),true);
                dl->AddText(ImVec2(x+15,y+9),IM_COL32(237,240,242,int(255*alpha)),entry.name);
                dl->AddText(ImVec2(x+15,y+30),accent,entry.enabled ? "Activado" : "Desactivado");
                dl->PopClipRect();
            }
            dl->PopClipRect();
        }
    };
}
