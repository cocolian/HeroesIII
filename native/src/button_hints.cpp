#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
std::string Desktop::originalButtonHint(const std::string& name,int normal)const{
 if(name=="iokay.def"||name=="iok6432.def")return "确认并关闭当前窗口（Enter）";
 if(name=="icancel.def")return "取消本次操作，返回上一界面（Esc）";
 if(name=="hsbtns.def")return "返回城镇（Esc）";
 if(name=="tpmage1.def")return "离开魔法行会，返回城镇（Esc）";
 if(name=="igpcrdiv.def")return normal==0?"减少选择数量（←）":"增加选择数量（→）";
 if(name=="ircbtns.def")return "选择当前库存、资源和部队空位允许的最大招募数量（End）";
 if(name=="ibuy30.def")return "支付费用并招募所选数量的部队（Enter）";
 if(name=="tpmrkb.def")return normal==3?"选择可交易的最大数量（End）":"按当前汇率交换所选资源（Enter）";
 if(name=="tpmrkbu1.def")return "赠送资源：当前版本尚未实现";
 if(name=="icm003.def")return "打开战斗与系统选项（O）";
 if(name=="icm011.def")return "选择下一支可以布阵的部队";if(name=="icm012.def")return "完成战术布阵并开始战斗";
 if(name=="icm001.def")return "投降：支付费用保留幸存部队，外交术降低费用；英雄可回酒馆重雇";
 if(name=="icm002.def")return "撤退：丢失部队，保留英雄的宝物和经验；战争枷锁禁止撤退";
 if(name=="icm004.def")return autoCombat_?"停止自动战斗，恢复手动操作（A）":"开始自动战斗（A）；再次点击可停止";
 if(name=="icm005.def")return "打开战斗魔法书（C）；需要魔法书，且轮到我方行动";
 if(name=="icm006.def")return "等待：本轮稍后行动（W）；每支部队每轮只能等待一次";
 if(name=="icm007.def")return "防御：结束本次行动并临时提高防御（D / 空格）";
 return {};
}
std::string Desktop::buttonHintAt(Point p)const{
 for(auto it=buttons_.rbegin();it!=buttons_.rend();++it)if(it->contains(p)){
  if(it->hint.empty())return {};
  return it->hint+(it->enabled?"":"\n当前不可用");
 }
 return {};
}
void Desktop::drawButtonHint(){
 hintBounds_.reset();
 auto description=buttonHintAt({mouseX_,mouseY_});
 const auto flags=SDL_GetWindowFlags(window_);
 if((!(flags&SDL_WINDOW_HIDDEN)&&!(flags&SDL_WINDOW_INPUT_FOCUS))||background_||mouseX_<0||mouseY_<0||mouseLeftDown_||(drag_&&dragged_)||marching_||touchPrimary_)description.clear();
 if(description!=hintText_||hintScene_!=scene||std::abs(mouseX_-hintAnchor_.x)+std::abs(mouseY_-hintAnchor_.y)>3||animationTime_<hintSince_){
  hintText_=description;hintScene_=scene;hintAnchor_={mouseX_,mouseY_};hintSince_=animationTime_;
 }
 if(description.empty()||(!rightHelp_&&animationTime_-hintSince_<450))return;
 auto size=canvasLogicalSize();constexpr int wrap=300;
 auto*surface=TTF_RenderUTF8_Blended_Wrapped(font(12),description.c_str(),{244,224,174,255},Uint32(wrap*pixelScale_));
 if(!surface)return;
 int width=int(std::ceil(surface->w/pixelScale_))+20,height=int(std::ceil(surface->h/pixelScale_))+16;SDL_FreeSurface(surface);
 width=std::min(width,size.x-12);height=std::min(height,size.y-12);
 int x=std::clamp(mouseX_+18,6,size.x-width-6),y=mouseY_+24;
 if(y+height>size.y-6)y=mouseY_-height-12;
 y=std::clamp(y,6,size.y-height-6);
 hintBounds_=SDL_Rect{x,y,width,height};
 panel(*hintBounds_);text(description,x+10,y+8,12,{244,224,174,255},wrap);
}
}
