#ifndef LOCAL_PLANNER_THETASTAR_H
#define LOCAL_PLANNER_THETASTAR_H

/// PROJECT
#include <path_follower/local_planner/local_planner_star.h>

class LocalPlannerThetaStar : virtual public LocalPlannerStar
{
public:
    LocalPlannerThetaStar();
private:
    bool tryForAlternative(LNode*& s_p, const std::vector<Constraint::Ptr>& constraints,
                           const std::vector<bool>& fconstraints);
    virtual double G(LNode*& current, LNode*& succ,
                     const std::vector<Constraint::Ptr>& constraints,
                     const std::vector<Scorer::Ptr>& scorer,
                     const std::vector<bool>& fconstraints,
                     const std::vector<double>& wscorer,
                     double& score) override;

    virtual void updateSucc(LNode*& current, LNode*& f_current, LNode& succ) override;
private:
    LNode alt;
};

#endif // LOCAL_PLANNER_THETASTAR_H
