/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzFramework/Math/MathUtils.h>
#include <EMotionFX/Source/EMotionFXConfig.h>
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphInstance.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/AnimGraphManager.h>
#include <EMotionFX/Source/BlendTreeSetTransformNode.h>
#include <EMotionFX/Source/Node.h>
#include <EMotionFX/Source/EMotionFXManager.h>


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSetTransformNode, AnimGraphAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSetTransformNode::UniqueData, AnimGraphObjectUniqueDataAllocator, 0)


    BlendTreeSetTransformNode::BlendTreeSetTransformNode()
        : AnimGraphNode()
        , m_transformSpace(TRANSFORM_SPACE_WORLD)
    {
        // setup the input ports
        InitInputPorts(4);
        SetupInputPort("Input Pose", INPUTPORT_POSE, AttributePose::TYPE_ID, PORTID_INPUT_POSE);
        SetupInputPortAsVector3("Translation", INPUTPORT_TRANSLATION, PORTID_INPUT_TRANSLATION);
        SetupInputPort("Rotation", INPUTPORT_ROTATION, MCore::AttributeQuaternion::TYPE_ID, PORTID_INPUT_ROTATION);
        SetupInputPortAsVector3("Scale", INPUTPORT_SCALE, PORTID_INPUT_SCALE);

        // setup the output ports
        InitOutputPorts(1);
        SetupOutputPortAsPose("Output Pose", OUTPUTPORT_RESULT, PORTID_OUTPUT_POSE);
    }


    BlendTreeSetTransformNode::~BlendTreeSetTransformNode()
    {
    }


    void BlendTreeSetTransformNode::Reinit()
    {
        AnimGraphNode::Reinit();

        const size_t numAnimGraphInstances = mAnimGraph->GetNumAnimGraphInstances();
        for (size_t i = 0; i < numAnimGraphInstances; ++i)
        {
            AnimGraphInstance* animGraphInstance = mAnimGraph->GetAnimGraphInstance(i);

            UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
            if (uniqueData)
            {
                uniqueData->m_mustUpdate = true;
                OnUpdateUniqueData(animGraphInstance);
            }
        }
    }


    bool BlendTreeSetTransformNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }


    const char* BlendTreeSetTransformNode::GetPaletteName() const
    {
        return "Set Transform";
    }


    AnimGraphObject::ECategory BlendTreeSetTransformNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_CONTROLLERS;
    }


    void BlendTreeSetTransformNode::Output(AnimGraphInstance* animGraphInstance)
    {
        ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
        AnimGraphPose* outputPose;

        // get the unique
        UniqueData* uniqueData = static_cast<UniqueData*>(FindUniqueNodeData(animGraphInstance));
        UpdateUniqueData(animGraphInstance, uniqueData);

        if (GetEMotionFX().GetIsInEditorMode())
        {
            SetHasError(animGraphInstance, uniqueData->m_nodeIndex == MCORE_INVALIDINDEX32);
        }

        OutputAllIncomingNodes(animGraphInstance);

        // make sure we have at least an input pose, otherwise output the bind pose
        if (GetInputPort(INPUTPORT_POSE).mConnection)
        {
            const AnimGraphPose* inputPose = GetInputPose(animGraphInstance, INPUTPORT_POSE)->GetValue();
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_RESULT)->GetValue();
            *outputPose = *inputPose;
        }
        else // init on the input pose
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_RESULT)->GetValue();
            outputPose->InitFromBindPose(actorInstance);
        }

        if (GetIsEnabled())
        {
            // get the local transform from our node
            if (uniqueData->m_nodeIndex != MCORE_INVALIDINDEX32)
            {
                Transform outputTransform;

                switch (m_transformSpace)
                {
                case TRANSFORM_SPACE_LOCAL:
                    outputPose->GetPose().GetLocalSpaceTransform(uniqueData->m_nodeIndex, &outputTransform);
                    break;
                case TRANSFORM_SPACE_WORLD:
                    outputPose->GetPose().GetWorldSpaceTransform(uniqueData->m_nodeIndex, &outputTransform);
                    break;
                case TRANSFORM_SPACE_MODEL:
                    outputPose->GetPose().GetModelSpaceTransform(uniqueData->m_nodeIndex, &outputTransform);
                    break;
                default:
                    outputTransform.Identity();
                    AZ_Assert(false, "Unhandled transform space");
                    break;
                }

                // process the translation
                AZ::Vector3 translation;
                if (TryGetInputVector3(animGraphInstance, INPUTPORT_TRANSLATION, translation))
                {
                    outputTransform.mPosition = translation;
                }

                // process the rotation
                if (GetInputPort(INPUTPORT_ROTATION).mConnection)
                {
                    const MCore::Quaternion& rotation = GetInputQuaternion(animGraphInstance, INPUTPORT_ROTATION)->GetValue();
                    outputTransform.mRotation = rotation;
                }

                // process the scale
                AZ::Vector3 scale;
                if (TryGetInputVector3(animGraphInstance, INPUTPORT_SCALE, scale))
                {
                    outputTransform.mScale = scale;
                }

                // update the transformation of the node
                switch (m_transformSpace)
                {
                case TRANSFORM_SPACE_LOCAL:
                    outputPose->GetPose().SetLocalSpaceTransform(uniqueData->m_nodeIndex, outputTransform);
                    break;
                case TRANSFORM_SPACE_WORLD:
                    outputPose->GetPose().SetWorldSpaceTransform(uniqueData->m_nodeIndex, outputTransform);
                    break;
                case TRANSFORM_SPACE_MODEL:
                    outputPose->GetPose().SetModelSpaceTransform(uniqueData->m_nodeIndex, outputTransform);
                    break;
                default:
                    outputTransform.Identity();
                    AZ_Assert(false, "Unhandled transform space");
                    break;
                }
            }
        }

        // visualize it
        if (GetEMotionFX().GetIsInEditorMode() && GetCanVisualize(animGraphInstance))
        {
            animGraphInstance->GetActorInstance()->DrawSkeleton(outputPose->GetPose(), mVisualizeColor);
        }
    }


    void BlendTreeSetTransformNode::UpdateUniqueData(AnimGraphInstance* animGraphInstance, UniqueData* uniqueData)
    {
        if (uniqueData->m_mustUpdate)
        {
            ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
            Actor* actor = actorInstance->GetActor();

            uniqueData->m_mustUpdate = false;
            uniqueData->m_nodeIndex  = MCORE_INVALIDINDEX32;

            if (m_nodeName.empty())
            {
                return;
            }

            const Node* node = actor->GetSkeleton()->FindNodeByName(m_nodeName.c_str());
            if (!node)
            {
                return;
            }

            uniqueData->m_nodeIndex  = node->GetNodeIndex();
        }
    }


    void BlendTreeSetTransformNode::OnUpdateUniqueData(AnimGraphInstance* animGraphInstance)
    {
        UniqueData* uniqueData = static_cast<UniqueData*>(animGraphInstance->FindUniqueObjectData(this));
        if (uniqueData == nullptr)
        {
            uniqueData = aznew UniqueData(this, animGraphInstance);
            animGraphInstance->RegisterUniqueObjectData(uniqueData);
        }

        UpdateUniqueData(animGraphInstance, uniqueData);
    }


    void BlendTreeSetTransformNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeSetTransformNode, AnimGraphNode>()
            ->Version(1)
            ->Field("nodeName", &BlendTreeSetTransformNode::m_nodeName)
            ->Field("transformSpace", &BlendTreeSetTransformNode::m_transformSpace)
        ;


        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        const AZ::VectorFloat maxVecFloat = AZ::VectorFloat(std::numeric_limits<float>::max());
        editContext->Class<BlendTreeSetTransformNode>("Set Transform Node", "Transform node attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ_CRC("ActorNode", 0x35d9eb50), &BlendTreeSetTransformNode::m_nodeName, "Node", "The node to apply the transform to.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeSetTransformNode::Reinit)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &BlendTreeSetTransformNode::m_transformSpace)
        ;
    }
} // namespace EMotionFX