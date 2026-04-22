
#include "MovieSceneBPLibrary.h"
#include "MiscBPLibrary.h"
#include "TKLibrary.h"


#include "EventHandlers/ISequenceDataEventHandler.h"
#include "EngineUtils.h"      
#include "LevelSequenceActor.h"
#include "LevelSequence.h"
#include "LevelSequenceModule.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneBindingProxy.h"
#include "MovieSceneGroomCacheSection.h"
#include "MovieSceneGeometryCacheSection.h"
#include "MovieSceneSection.h"

#include "LevelSequencePlayer.h"
#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "UniversalObjectLocatorResolveParams.h"

#include "GroomCache.h"
#include "GroomComponent.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "MovieSceneGeometryCacheTrack.h"
#include "MovieSceneGroomCacheTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"

#include "ISequencerModule.h"
#include "ISequencer.h"


#define LOCTEXT_NAMESPACE "MovieSceneBPLibrary"

FGuid UMovieSceneBPLibrary::GetActorGuidFromLevelSequence(AActor* Actor, ULevelSequence* LevelSequence, bool& bSuccess, FString& InfoMessage)
{
	if (Actor == nullptr || LevelSequence == nullptr)
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Get Actor Guid From Level Seuquence Failed - Actor or LevelSequence isn't valid. '%s'"));
		return FGuid();
	}

	ALevelSequenceActor* TargetActor = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(Actor->GetWorld()); It; ++It)
	{
		if (It->GetSequence() == LevelSequence)
		{
			TargetActor = *It;
			break;
		}
	}

	/*TSharedRef<UE::MovieScene::FSharedPlaybackState> SharedPlaybackState =
		TargetActor->GetSequencePlayer()->GetSharedPlaybackState();*/
	FGuid Guid = LevelSequence->FindBindingFromObject(Actor, Actor->GetWorld());


	bSuccess = Guid.IsValid();
	FString TF = bSuccess ? "Succeeded" : "Failed";
	InfoMessage = FString::Printf(TEXT("Get Actor Guid From Level Sequence %s"), *TF);
	return Guid;
}

FGuid UMovieSceneBPLibrary::AddActorIntoLevelSequence(AActor* Actor, ULevelSequence* LevelSequence, bool& bSuccess, FString& InfoMessage)
{
	FGuid Guid = GetActorGuidFromLevelSequence(Actor, LevelSequence, bSuccess, InfoMessage);
	if (bSuccess)
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Add Actor Into Level Sequence Failed - Actor %s is already in the level sequence."), *Actor->GetName());
		return Guid;
	}

	UMovieSceneSequence* MovieSceneSequence = Cast<UMovieSceneSequence>(LevelSequence);
	if(!MovieSceneSequence)
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Add Actor Into Level Sequence Failed - MovieSceneSequence is nullptr."));
		return Guid;
	}

	MovieSceneSequence->Modify();
	Guid = MovieSceneSequence->CreatePossessable(Actor);
	bSuccess = true;
	InfoMessage = FString::Printf(TEXT("Add Actor Into Level Sequence Succeeded."));
	return Guid;
}



void UMovieSceneBPLibrary::RemoveActorFromLevelSequence(AActor* Actor, ULevelSequence* LevelSequence, bool& bSuccess, FString& InfoMessage)
{
	FGuid Guid = GetActorGuidFromLevelSequence(Actor, LevelSequence, bSuccess, InfoMessage);
	if (!Guid.IsValid())
	{
		return;
	}

	LevelSequence->UnbindPossessableObjects(Guid);
	bSuccess = LevelSequence->GetMovieScene()->RemovePossessable(Guid);
	FString TF = bSuccess ? "Succeeded" : "Failed";
	InfoMessage = FString::Printf(TEXT("Remove Actor From Level Sequence %s"), *TF);

}

AActor* UMovieSceneBPLibrary::GetActorFromGuidInLevelSequence(FGuid Guid, ULevelSequence* LevelSequence, bool& bSuccess, FString& InfoMessage)
{
	if (!LevelSequence)
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Get Actor From Guid In LevelSequence Failed - LevelSequence is nullptr."));
		return nullptr;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Get World Failed - World is nullptr."));
		return nullptr;
	}
	TArray<UObject*, TInlineAllocator<1>> OutObjects;

	UE::UniversalObjectLocator::FResolveParams ResolveParams(World);

	/*ALevelSequenceActor* TargetActor = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(LevelSequence->GetWorld()); It; ++It)
	{
		if (It->GetSequence() == LevelSequence)
		{
			TargetActor = *It;
			break;
		}
	}*/
	/*TSharedRef<UE::MovieScene::FSharedPlaybackState> SharedPlaybackState =
		TargetActor->GetSequencePlayer()->GetSharedPlaybackState();*/

	LevelSequence->LocateBoundObjects(Guid, ResolveParams, nullptr, OutObjects);
	if (OutObjects.Num() > 0)
	{
		bSuccess = true;
		InfoMessage = FString::Printf(TEXT("Get Actor From Guid In LevelSequence Succeeded!"));
		return Cast<AActor>(OutObjects[0]);
	}
	else
	{
		bSuccess = false;
		InfoMessage = FString::Printf(TEXT("Get Actor From Guid In LevelSequence Failed With Guid '%s'"), *Guid.ToString());
		return nullptr;
	}
}

void UMovieSceneBPLibrary::SetGroomCache(UMovieSceneGroomCacheSection* GroomCacheSection, UGroomCache* GroomCache)
{
	if (GroomCacheSection == nullptr || GroomCache == nullptr)
		return;

	GroomCacheSection->Modify();
	FMovieSceneGroomCacheParams& Params = GroomCacheSection->Params;
	Params.GroomCache = GroomCache;
}

int32 UMovieSceneBPLibrary::GetGroomCacheEndFrame(UGroomCache* GroomCache)
{
	if (GroomCache != nullptr)
	{
		return GroomCache->GetEndFrame();
	}
	return 0;
}



void UMovieSceneBPLibrary::RefreshSequencer(ULevelSequence* LevelSequence)
{
	// Load the Sequencer module
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

	// Create a new Sequencer instance for the Level Sequence
	FSequencerInitParams SequencerInitParams;
	SequencerInitParams.RootSequence = LevelSequence;
	SequencerInitParams.ToolkitHost = nullptr;

	TSharedPtr<ISequencer> Sequencer = SequencerModule.CreateSequencer(SequencerInitParams);
	if (Sequencer.IsValid())
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		Sequencer->UpdatePlaybackRange();
	}
}


bool UMovieSceneBPLibrary::ImportActorsToSequencer(const TArray<AActor*>& InActors, UMovieSceneSequence* InMovieSceneSequence, const FString& InRootFolderName)
{
	if (InActors.Num() == 0 || InMovieSceneSequence == nullptr)
	{
		UMiscBPLibrary::NotificationText(LOCTEXT("ImportActorsFail", "InActors is empty or InMovieSceneSequence is nullptr."), true, false);
		return false;
	}



	bool bFolderExist = false;
	bool bExecuted = true;


	for (AActor* Actor : InActors)
	{
		bool bSuccess;
		FString Info;
		FGuid Guid = UMovieSceneBPLibrary::AddActorIntoLevelSequence(Actor, Cast<ULevelSequence>(InMovieSceneSequence), bSuccess, Info);
		if (!bSuccess)
		{
			UMiscBPLibrary::Notification(Info, true, false);
			continue;
		}
		
		bExecuted = true;
	
		if (!InRootFolderName.IsEmpty() && !bFolderExist)
		{
			TArrayView<UMovieSceneFolder* const> RootFolders = InMovieSceneSequence->GetMovieScene()->GetRootFolders();
			
			UMovieSceneFolder* RootFolder = nullptr;
			if (!bFolderExist)
			{
				for (UMovieSceneFolder* Folder : RootFolders)
				{
					bFolderExist = Folder->GetFolderName().ToString() == InRootFolderName ? true : false;
				}
			}
			
			if (!bFolderExist)
			{		
				InMovieSceneSequence->GetMovieScene()->Modify();
				RootFolder = NewObject<UMovieSceneFolder>(InMovieSceneSequence->GetMovieScene(), NAME_None, RF_Transactional);
				RootFolder->SetFolderName(FName(*InRootFolderName));
				InMovieSceneSequence->GetMovieScene()->AddRootFolder(RootFolder);							
			}

			RootFolder->AddChildObjectBinding(Guid);
		}

		const TSet<UActorComponent*> Components = Actor->GetComponents();
		for (UActorComponent* Comp : Components)
		{
			//Add Skeletal Animation Track
			if (Comp->GetClass() == USkeletalMeshComponent::StaticClass())
			{
				FGuid NewGuid = InMovieSceneSequence->CreatePossessable(Comp);
				UMovieSceneSkeletalAnimationTrack* CompTrack = NewObject<UMovieSceneSkeletalAnimationTrack>(InMovieSceneSequence->GetMovieScene(), UMovieSceneSkeletalAnimationTrack::StaticClass(), NAME_None, RF_Transactional);					
				if (CompTrack)
				{
					InMovieSceneSequence->GetMovieScene()->AddGivenTrack(CompTrack, NewGuid);
					CompTrack->SetDisplayName(FText::FromString(Comp->GetName()));
					UMovieSceneSection* Section = AddSection(CompTrack);
					UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
					USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Comp);

					if (AnimSection && SkelComp)
					{

						SkelComp->AnimationData.bSavedPlaying = false;
						TObjectPtr<UAnimSequenceBase> AnimAsset = Cast<UAnimSequenceBase>(SkelComp->AnimationData.AnimToPlay);
						if (AnimAsset)
						{
							AnimSection->Params.Animation = AnimAsset;
							//Set Range
							SetSectionRange(InMovieSceneSequence, AnimSection, 0, AnimAsset->GetNumberOfSampledKeys() - 1);
						}
					}
				}
			}
			//Add Geometry Cache Track
			else if (Comp->GetClass() == UGeometryCacheComponent::StaticClass())
			{
				FGuid NewGuid = InMovieSceneSequence->CreatePossessable(Comp);
				UMovieSceneGeometryCacheTrack* CompTrack = NewObject<UMovieSceneGeometryCacheTrack>(InMovieSceneSequence->GetMovieScene(), UMovieSceneGeometryCacheTrack::StaticClass(), NAME_None, RF_Transactional);
				if (CompTrack)
				{
					InMovieSceneSequence->GetMovieScene()->AddGivenTrack(CompTrack, NewGuid);
					CompTrack->SetDisplayName(FText::FromString(Comp->GetName()));
					UMovieSceneSection* Section = AddSection(CompTrack);
					UMovieSceneGeometryCacheSection* GeoSection = Cast<UMovieSceneGeometryCacheSection>(Section);
					UGeometryCacheComponent* GeoComp = Cast<UGeometryCacheComponent>(Comp);

					if (GeoSection && GeoComp)
					{
						GeoComp->SetManualTick(true);
						UGeometryCache* GeometryCache = GeoComp->GetGeometryCache();
						if (GeometryCache)
						{						
							GeoSection->Params.GeometryCacheAsset = GeometryCache;
							//Set Range
							int32 EndFrame = GeometryCache->GetEndFrame() - GeometryCache->GetStartFrame();
							SetSectionRange(InMovieSceneSequence, GeoSection, 0, EndFrame);
						}
					}
				}
			}
			//Add Groom Cache Track
			else if (Comp->GetClass() == UGroomComponent::StaticClass())
			{
				FGuid NewGuid = InMovieSceneSequence->CreatePossessable(Comp);
				UMovieSceneGroomCacheTrack* CompTrack = NewObject<UMovieSceneGroomCacheTrack>(InMovieSceneSequence->GetMovieScene(), UMovieSceneGroomCacheTrack::StaticClass(), NAME_None, RF_Transactional);
				if (CompTrack)
				{
					InMovieSceneSequence->GetMovieScene()->AddGivenTrack(CompTrack, NewGuid);
					CompTrack->SetDisplayName(FText::FromString(Comp->GetName()));
					UMovieSceneSection* Section = AddSection(CompTrack);
					UMovieSceneGroomCacheSection* GroomSection = Cast<UMovieSceneGroomCacheSection>(Section);
					UGroomComponent* GroomComp = Cast<UGroomComponent>(Comp);

					if (GroomSection && GroomComp)
					{
						GroomComp->SetManualTick(true);
						UGroomCache* GroomCache = GroomComp->GetGroomCache();
						if (GroomCache)
						{
							GroomSection->Params.GroomCache = GroomCache;
							GroomSection->Modify();
							//Set Range
							SetSectionRange(InMovieSceneSequence, GroomSection, 0, GroomCache->GetEndFrame() - GroomCache->GetStartFrame());						
						}
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Current component is not supported!"));
			}
		}
	}

	//Sort Camera Cut Track
	UMovieSceneTrack* CameraCutTrack = InMovieSceneSequence->GetMovieScene()->GetCameraCutTrack();
	if (CameraCutTrack && bExecuted)
		InMovieSceneSequence->GetMovieScene()->EventHandlers.Trigger(&UE::MovieScene::ISequenceDataEventHandler::OnTrackAdded, CameraCutTrack);

	return true;
}

void UMovieSceneBPLibrary::SetSectionRange(UMovieSceneSequence* InMovieSceneSequence, UMovieSceneSection* InSection, int32 StartFrame, int32 EndFrame)
{
	if (!InMovieSceneSequence->GetMovieScene() || !InSection)
		return;

	FFrameRate DisplayRate = InMovieSceneSequence->GetMovieScene()->GetDisplayRate();
	TRange<FFrameNumber> NewRange;
	NewRange.SetLowerBound(TRangeBound<FFrameNumber>::Inclusive(ConvertFrameTime(StartFrame, DisplayRate, InMovieSceneSequence->GetMovieScene()->GetTickResolution()).FrameNumber));
	NewRange.SetUpperBound(TRangeBound<FFrameNumber>::Exclusive(ConvertFrameTime(EndFrame, DisplayRate, InMovieSceneSequence->GetMovieScene()->GetTickResolution()).FrameNumber));

	if (NewRange.GetLowerBound().IsOpen() || NewRange.GetUpperBound().IsOpen() || NewRange.GetLowerBoundValue() <= NewRange.GetUpperBoundValue())
	{
		InSection->SetRange(NewRange);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid range specified"), ELogVerbosity::Error);
	}
}

UMovieSceneSection* UMovieSceneBPLibrary::AddSection(UMovieSceneTrack* Track)
{
	if (!Track)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call AddSection on a null track"), ELogVerbosity::Error);
		return nullptr;
	}

	UMovieSceneSection* NewSection = Track->CreateNewSection();

	if (NewSection)
	{
		Track->Modify();
		Track->AddSection(*NewSection);
	}

	return NewSection;
}

void UMovieSceneBPLibrary::SetRollFrames(UMovieSceneSequence* InMovieSceneSequence, UMovieSceneSection* InSection, int32 PreRollFrames, int32 PostRollFrames)
{
	FFrameRate TickResolution = InMovieSceneSequence->GetMovieScene()->GetTickResolution();
	FFrameRate DisplayRate = InMovieSceneSequence->GetMovieScene()->GetDisplayRate();

	FFrameNumber PreRollTicks = ConvertFrameTime(FFrameTime(PreRollFrames), DisplayRate, TickResolution).GetFrame();
	FFrameNumber PostRollTicks = ConvertFrameTime(FFrameTime(PostRollFrames), DisplayRate, TickResolution).GetFrame();
	InSection->SetPreRollFrames(PreRollTicks.Value);
	InSection->SetPostRollFrames(PostRollTicks.Value);
}

FGuid UMovieSceneBPLibrary::CreateNewSpawnable(UMovieSceneSequence* InMovieSceneSequence, UObject* SourceObject)
{
	if (!InMovieSceneSequence || !SourceObject)
	{
		FFrame::KismetExecutionMessage(TEXT("Either InMovieSceneSequence or SourceObject is nullptr."), ELogVerbosity::Error);
		return FGuid();
	}

	FGuid NewGuid;
	TArray<TSharedRef<IMovieSceneObjectSpawner>> ObjectSpawners;
	// In order to create a spawnable, we have to instantiate all the relevant object spawners for level sequences, and try to create a spawnable from each
	FLevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<FLevelSequenceModule>("LevelSequence");
	LevelSequenceModule.GenerateObjectSpawners(ObjectSpawners);
	// The first object spawner to return a valid result will win
	for (TSharedRef<IMovieSceneObjectSpawner> Spawner : ObjectSpawners)
	{
		TValueOrError<FNewSpawnable, FText> Result = Spawner->CreateNewSpawnableType(*SourceObject, *InMovieSceneSequence->GetMovieScene(), nullptr);
		if (Result.IsValid())
		{
			FNewSpawnable& NewSpawnable = Result.GetValue();
			NewSpawnable.Name = MovieSceneHelpers::MakeUniqueSpawnableName(InMovieSceneSequence->GetMovieScene(), NewSpawnable.Name);
			NewGuid = InMovieSceneSequence->GetMovieScene()->AddSpawnable(NewSpawnable.Name, *NewSpawnable.ObjectTemplate);
			UMovieSceneSpawnTrack* NewSpawnTrack = InMovieSceneSequence->GetMovieScene()->AddTrack<UMovieSceneSpawnTrack>(NewGuid);
			if (NewSpawnTrack)
			{
				NewSpawnTrack->AddSection(*NewSpawnTrack->CreateNewSection());
			}
			return NewGuid;
		}
	}

	return NewGuid;
}

#undef LOCTEXT_NAMESPACE
