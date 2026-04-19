#include "cocos/ui/editBox/TabIndexUtil.h"

#include <algorithm>

#include "cocos/ui/editBox/EditBox.h"
#include "cocos/ui/editBox/EditBoxImplBase.h"

namespace cc {

ccstd::vector<EditBoxImplBase *> &TabIndexUtil::list() {
    static ccstd::vector<EditBoxImplBase *> s;
    return s;
}

void TabIndexUtil::add(EditBoxImplBase *impl) {
    if (!impl) return;
    auto &ls = list();
    if (std::find(ls.begin(), ls.end(), impl) == ls.end()) {
        ls.push_back(impl);
    }
}

void TabIndexUtil::remove(EditBoxImplBase *impl) {
    if (!impl) return;
    auto &ls = list();
    ls.erase(std::remove(ls.begin(), ls.end(), impl), ls.end());
}

void TabIndexUtil::resort() {
    auto &ls = list();
    std::sort(ls.begin(), ls.end(), [](EditBoxImplBase *a, EditBoxImplBase *b) {
        EditBox *da = a ? a->getDelegate() : nullptr;
        EditBox *db = b ? b->getDelegate() : nullptr;
        const int ia = da ? da->getTabIndex() : 0;
        const int ib = db ? db->getTabIndex() : 0;
        return ia < ib;
    });
}

void TabIndexUtil::next(EditBoxImplBase *from) {
    if (!from) return;
    auto &ls = list();
    from->setFocus(false);
    auto it = std::find(ls.begin(), ls.end(), from);
    if (it == ls.end()) return;
    auto nextIt = std::next(it);
    if (nextIt == ls.end()) return;
    EditBoxImplBase *nextImpl = *nextIt;
    EditBox *delegate = nextImpl ? nextImpl->getDelegate() : nullptr;
    if (delegate && delegate->getTabIndex() >= 0) {
        nextImpl->setFocus(true);
    }
}

}  // namespace cc
